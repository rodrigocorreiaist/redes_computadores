#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "protocol_tcp.h"
#include "utils.h"

#define BUFFER_SIZE 1024
#define MAX_FILE_SIZE 10485760  // 10MB

extern User *user_list;
extern Event *event_list;
extern Reservation *reservation_list;
extern int total_events;

// Helper to find user
static User* find_user_tcp(const char *UID) {
    User *curr = user_list;
    while (curr) {
        if (strcmp(curr->UID, UID) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

// Helper to find event
static Event* find_event_tcp(const char *EID) {
    Event *curr = event_list;
    while (curr) {
        if (strcmp(curr->EID, EID) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

// Protocol: CRE UID pass name date time cap filename filesize\n <binary>
// Response: RCE OK EID\n | RCE NOK\n | RCE ERR\n
static void handle_cre(int client_fd, const char *buffer) {
    char UID[7], password[9], name[11], date[11], time[6], filename[256];
    int capacity;
    size_t filesize;
    
    // Parse command line
    int parsed = sscanf(buffer, "CRE %6s %8s %10s %10s %5s %d %255s %zu",
                        UID, password, name, date, time, &capacity, filename, &filesize);
    
    if (parsed != 8) {
        send_all_tcp(client_fd, "RCE ERR\n", 8);
        return;
    }
    
    // Validate all fields
    if (!validate_uid(UID) || !validate_password(password) || 
        !validate_event_name(name) || !validate_date(date) || 
        !validate_time(time) || capacity < 10 || capacity > 999 ||
        filesize == 0 || filesize > MAX_FILE_SIZE) {
        send_all_tcp(client_fd, "RCE ERR\n", 8);
        return;
    }
    
    // Check user exists and password is correct
    User *u = find_user_tcp(UID);
    if (!u || strcmp(u->password, password) != 0) {
        send_all_tcp(client_fd, "RCE NOK\n", 8);
        return;
    }
    
    // Check if user is logged in
    if (!u->loggedIn) {
        send_all_tcp(client_fd, "RCE NLG\n", 8);
        return;
    }
    
    // Receive binary file data
    char *file_data = (char *)malloc(filesize);
    if (!file_data) {
        send_all_tcp(client_fd, "RCE NOK\n", 8);
        return;
    }
    
    if (recv_all_tcp(client_fd, file_data, filesize) < 0) {
        free(file_data);
        send_all_tcp(client_fd, "RCE ERR\n", 8);
        return;
    }
    
    // Create event
    int eid_num = create_event(UID, name, date, time, capacity, filename, filesize);
    if (eid_num < 0) {
        free(file_data);
        send_all_tcp(client_fd, "RCE NOK\n", 8);
        return;
    }
    
    // Save file to EVENTS directory
    char eid_str[16];
    snprintf(eid_str, sizeof(eid_str), "%03d", eid_num);
    
    mkdir("EVENTS", 0755);  // Create directory if doesn't exist
    
    char filepath[300];
    snprintf(filepath, sizeof(filepath), "EVENTS/%s_%s", eid_str, filename);
    
    FILE *fp = fopen(filepath, "wb");
    if (fp) {
        fwrite(file_data, 1, filesize, fp);
        fclose(fp);
    }
    free(file_data);
    
    // Send success response
    char response[32];
    snprintf(response, sizeof(response), "RCE OK %s\n", eid_str);
    send_all_tcp(client_fd, response, strlen(response));
}

// Protocol: CLS UID pass EID\n
// Response: RCL OK\n | RCL NOK\n | RCL ERR\n | RCL EID\n | RCL USR\n
static void handle_cls(int client_fd, const char *buffer) {
    char UID[7], password[9], EID[4];
    
    if (sscanf(buffer, "CLS %6s %8s %3s", UID, password, EID) != 3) {
        send_all_tcp(client_fd, "RCL ERR\n", 8);
        return;
    }
    
    if (!validate_uid(UID) || !validate_password(password) || !validate_eid(EID)) {
        send_all_tcp(client_fd, "RCL ERR\n", 8);
        return;
    }
    
    // Check user
    User *u = find_user_tcp(UID);
    if (!u || strcmp(u->password, password) != 0) {
        send_all_tcp(client_fd, "RCL USR\n", 8);
        return;
    }
    
    if (!u->loggedIn) {
        send_all_tcp(client_fd, "RCL NLG\n", 8);
        return;
    }
    
    // Check event
    Event *e = find_event_tcp(EID);
    if (!e) {
        send_all_tcp(client_fd, "RCL EID\n", 8);
        return;
    }
    
    // Check ownership
    if (strcmp(e->owner_UID, UID) != 0) {
        send_all_tcp(client_fd, "RCL USR\n", 8);
        return;
    }
    
    // Check if already closed or sold out
    if (e->status == 1) {
        send_all_tcp(client_fd, "RCL OK\n", 7);  // Already closed, OK
        return;
    }
    
    if (e->status == 2) {
        send_all_tcp(client_fd, "RCL FUL\n", 8);  // Sold out
        return;
    }
    
    // Close event
    int result = close_event(UID, EID);
    if (result == 0) {
        send_all_tcp(client_fd, "RCL OK\n", 7);
    } else {
        send_all_tcp(client_fd, "RCL NOK\n", 8);
    }
}

// Protocol: LST\n
// Response: RLS OK N [ EID name date time capacity reserved ]\n | RLS NOK\n
static void handle_lst(int client_fd, const char *buffer) {
    char response[BUFFER_SIZE * 4];
    int count = 0;
    Event *e = event_list;
    
    // Count all events
    while (e) {
        count++;
        e = e->next;
    }
    
    if (count == 0) {
        send_all_tcp(client_fd, "RLS NOK\n", 8);
        return;
    }
    
    // Build response
    char temp[BUFFER_SIZE];
    snprintf(response, sizeof(response), "RLS OK %d", count);
    
    e = event_list;
    while (e) {
        // Format: EID name date time capacity reserved
        snprintf(temp, sizeof(temp), " %s %s %s %s %d %d",
                 e->EID, e->name, e->date, e->time, 
                 e->attendance_size, e->seats_reserved);
        strcat(response, temp);
        e = e->next;
    }
    strcat(response, "\n");
    
    send_all_tcp(client_fd, response, strlen(response));
}

// Protocol: SED EID\n
// Response: RSE OK name date time capacity reserved filename filesize\n <binary> | RSE NOK\n
static void handle_sed(int client_fd, const char *buffer) {
    char EID[4];
    
    if (sscanf(buffer, "SED %3s", EID) != 1) {
        send_all_tcp(client_fd, "RSE ERR\n", 8);
        return;
    }
    
    if (!validate_eid(EID)) {
        send_all_tcp(client_fd, "RSE ERR\n", 8);
        return;
    }
    
    // Find event
    Event *e = find_event_tcp(EID);
    if (!e) {
        send_all_tcp(client_fd, "RSE NOK\n", 8);
        return;
    }
    
    // Build file path
    char filepath[300];
    snprintf(filepath, sizeof(filepath), "EVENTS/%s_%s", EID, e->filename);
    
    // Read file
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        send_all_tcp(client_fd, "RSE NOK\n", 8);
        return;
    }
    
    // Get file size
    fseek(fp, 0, SEEK_END);
    size_t fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    // Read file data
    char *file_data = (char *)malloc(fsize);
    if (!file_data) {
        fclose(fp);
        send_all_tcp(client_fd, "RSE NOK\n", 8);
        return;
    }
    
    size_t read_size = fread(file_data, 1, fsize, fp);
    fclose(fp);
    
    if (read_size != fsize) {
        free(file_data);
        send_all_tcp(client_fd, "RSE NOK\n", 8);
        return;
    }
    
    // Send response header
    char response[512];
    snprintf(response, sizeof(response), "RSE OK %s %s %s %d %d %s %zu %d\n",
             e->name, e->date, e->time, e->attendance_size, e->seats_reserved,
             e->filename, fsize, e->status);
    
    send_all_tcp(client_fd, response, strlen(response));
    
    // Send binary file data
    send_all_tcp(client_fd, file_data, fsize);
    
    free(file_data);
}

// Protocol: RID UID pass EID seats\n
// Response: RRI ACC\n | RRI NOK\n | RRI ERR\n | RRI CLS\n | RRI FUL\n
static void handle_rid(int client_fd, const char *buffer) {
    char UID[7], password[9], EID[4];
    int seats;
    
    if (sscanf(buffer, "RID %6s %8s %3s %d", UID, password, EID, &seats) != 4) {
        send_all_tcp(client_fd, "RRI ERR\n", 8);
        return;
    }
    
    if (!validate_uid(UID) || !validate_password(password) || 
        !validate_eid(EID) || seats <= 0) {
        send_all_tcp(client_fd, "RRI ERR\n", 8);
        return;
    }
    
    // Check user
    User *u = find_user_tcp(UID);
    if (!u || strcmp(u->password, password) != 0) {
        send_all_tcp(client_fd, "RRI NOK\n", 8);
        return;
    }
    
    if (!u->loggedIn) {
        send_all_tcp(client_fd, "RRI NLG\n", 8);
        return;
    }
    
    // Reserve seats
    int result = reserve_seats(UID, EID, seats);
    
    if (result == 0) {
        send_all_tcp(client_fd, "RRI ACC\n", 8);
    } else if (result == -1) {
        send_all_tcp(client_fd, "RRI NOK\n", 8);  // Event doesn't exist
    } else if (result == -2) {
        send_all_tcp(client_fd, "RRI CLS\n", 8);  // Event closed
    } else if (result == -3 || result == -4) {
        send_all_tcp(client_fd, "RRI FUL\n", 8);  // Sold out or not enough seats
    } else {
        send_all_tcp(client_fd, "RRI NOK\n", 8);
    }
}

// Protocol: CPS UID oldPassword newPassword\n
// Response: RCP OK\n | RCP NOK\n | RCP ERR\n | RCP NLG\n | RCP NID\n
static void handle_cps(int client_fd, const char *buffer) {
    char UID[7], oldPassword[9], newPassword[9];
    
    // Parse command: CPS UID oldPassword newPassword
    if (sscanf(buffer, "CPS %6s %8s %8s", UID, oldPassword, newPassword) != 3) {
        send_all_tcp(client_fd, "RCP ERR\n", 8);
        return;
    }
    
    // Validate format
    if (!validate_uid(UID) || !validate_password(oldPassword) || !validate_password(newPassword)) {
        send_all_tcp(client_fd, "RCP ERR\n", 8);
        return;
    }
    
    // Find user
    User *u = find_user_tcp(UID);
    
    // Check if user exists
    if (!u) {
        send_all_tcp(client_fd, "RCP NID\n", 8); // Not IDentified
        return;
    }
    
    // Check if user is logged in
    if (!u->loggedIn) {
        send_all_tcp(client_fd, "RCP NLG\n", 8); // Not Logged in
        return;
    }
    
    // Check if old password matches
    if (strcmp(u->password, oldPassword) != 0) {
        send_all_tcp(client_fd, "RCP NOK\n", 8); // Password incorrect
        return;
    }
    
    // Update password
    strcpy(u->password, newPassword);
    send_all_tcp(client_fd, "RCP OK\n", 7);
}

void process_tcp_command(int client_fd, int verbose_mode) {
    char buffer[8192];  // Larger buffer for commands with file metadata
    
    // Read command line (up to newline)
    int n = recv_line_tcp(client_fd, buffer, sizeof(buffer));
    if (n <= 0) { 
        close(client_fd); 
        return; 
    }

    char cmd[4] = "";
    char param[7] = "";
    if (sscanf(buffer, "%3s", cmd) != 1) {
        send_all_tcp(client_fd, "ERR\n", 4);
        close(client_fd);
        return;
    }
    
    // Extract parameter (UID or EID) for verbose logging
    if (strlen(buffer) > 4) {
        sscanf(buffer + 4, "%6s", param);
    }
    
    if (verbose_mode) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        if (getpeername(client_fd, (struct sockaddr*)&client_addr, &addr_len) == 0) {
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            
            if (strcmp(cmd, "SED") == 0) {
                printf("[TCP] Received %s from client %s:%d (EID: %s)\n", 
                       cmd, client_ip, ntohs(client_addr.sin_port), strlen(param) ? param : "N/A");
            } else {
                printf("[TCP] Received %s from client %s:%d (UID: %s)\n", 
                       cmd, client_ip, ntohs(client_addr.sin_port), strlen(param) ? param : "N/A");
            }
        } else {
            if (strcmp(cmd, "SED") == 0) {
                printf("[TCP] Received %s (EID: %s)\n", cmd, strlen(param) ? param : "N/A");
            } else {
                printf("[TCP] Received %s (UID: %s)\n", cmd, strlen(param) ? param : "N/A");
            }
        }
    }

    if (!strcmp(cmd, "CRE")) handle_cre(client_fd, buffer);
    else if (!strcmp(cmd, "CLS")) handle_cls(client_fd, buffer);
    else if (!strcmp(cmd, "LST")) handle_lst(client_fd, buffer);
    else if (!strcmp(cmd, "SED")) handle_sed(client_fd, buffer);
    else if (!strcmp(cmd, "RID")) handle_rid(client_fd, buffer);
    else if (!strcmp(cmd, "CPS")) handle_cps(client_fd, buffer);
    else send_all_tcp(client_fd, "ERR\n", 4);

    close(client_fd);
}
