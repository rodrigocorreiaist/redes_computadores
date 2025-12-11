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
#include "storage.h"

#define BUFFER_SIZE 1024
#define MAX_FILE_SIZE 10485760  // 10MB

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
    if (!storage_user_exists(UID) || !storage_check_password(UID, password)) {
        send_all_tcp(client_fd, "RCE NOK\n", 8);
        return;
    }
    
    // Check if user is logged in
    if (!storage_is_logged_in(UID)) {
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
    char eid[4];
    if (storage_create_event(UID, name, date, time, capacity, filename, file_data, filesize, eid) != 0) {
        free(file_data);
        send_all_tcp(client_fd, "RCE NOK\n", 8);
        return;
    }
    
    free(file_data);
    char response[32];
    snprintf(response, sizeof(response), "RCE OK %s\n", eid);
    send_all_tcp(client_fd, response, strlen(response));
}

// Protocol: CLS UID pass EID
// Response: RCL OK\n | RCL NOK\n | RCL EOW\n | RCL CLO\n | RCL ERR\n
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
    
    if (!storage_user_exists(UID)) {
        send_all_tcp(client_fd, "RCL NOK\n", 8);
        return;
    }
    
    if (!storage_check_password(UID, password)) {
        send_all_tcp(client_fd, "RCL NOK\n", 8); 
        return;
    }
    
    if (!storage_is_logged_in(UID)) {
        send_all_tcp(client_fd, "RCL NLG\n", 8);
        return;
    }
    
    int res = storage_close_event(UID, EID);
    if (res == 0) send_all_tcp(client_fd, "RCL OK\n", 7);
    else if (res == -1) send_all_tcp(client_fd, "RCL NOE\n", 8); // Event not found
    else if (res == -2) send_all_tcp(client_fd, "RCL EOW\n", 8); // Not owner
    else send_all_tcp(client_fd, "RCL CLO\n", 8); // Already closed (or other state)
}

// Protocol: LST
// Response: RLS OK [EID name state date]*\n | RLS NOK\n
static void handle_lst(int client_fd) {
    char list_buffer[60000];
    if (storage_list_events(list_buffer, sizeof(list_buffer)) != 0) {
        send_all_tcp(client_fd, "RLS NOK\n", 8);
        return;
    }
    
    if (strlen(list_buffer) == 0) {
        send_all_tcp(client_fd, "RLS OK\n", 7);
    } else {
        char header[32];
        snprintf(header, sizeof(header), "RLS OK");
        send_all_tcp(client_fd, header, strlen(header));
        send_all_tcp(client_fd, list_buffer, strlen(list_buffer));
        send_all_tcp(client_fd, "\n", 1);
    }
}

// Protocol: SED EID
// Response: RSE OK UID name date time cap reserved filename filesize\n <binary>
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
    
    char uid[7], name[100], date[11], time[6], fname[100];
    int attendance, reserved;
    size_t fsize;
    
    if (storage_get_event_details(EID, uid, name, date, time, &attendance, &reserved, fname, &fsize) != 0) {
        send_all_tcp(client_fd, "RSE NOK\n", 8);
        return;
    }
    
    char *file_data = (char *)malloc(fsize);
    if (!file_data) {
        send_all_tcp(client_fd, "RSE NOK\n", 8);
        return;
    }
    
    if (storage_get_event_file_data(EID, fname, file_data, fsize) != 0) {
        free(file_data);
        send_all_tcp(client_fd, "RSE NOK\n", 8);
        return;
    }
    
    char response[512];
    // Spec: RSE status [UID name event_date attendance_size Seats_reserved Fname Fsize Fdata]
    snprintf(response, sizeof(response), "RSE OK %s %s %s %s %d %d %s %zu ", 
             uid, name, date, time, attendance, reserved, fname, fsize);
             
    send_all_tcp(client_fd, response, strlen(response));
    send_all_tcp(client_fd, file_data, fsize);
    send_all_tcp(client_fd, "\n", 1);
    
    free(file_data);
}

// Protocol: RID UID pass EID num_seats
// Response: RRI OK\n | RRI NOK\n | RRI ERR\n
static void handle_rid(int client_fd, const char *buffer) {
    char UID[7], password[9], EID[4];
    int num_seats;
    
    if (sscanf(buffer, "RID %6s %8s %3s %d", UID, password, EID, &num_seats) != 4) {
        send_all_tcp(client_fd, "RRI ERR\n", 8);
        return;
    }
    
    if (!validate_uid(UID) || !validate_password(password) || !validate_eid(EID) || num_seats <= 0) {
        send_all_tcp(client_fd, "RRI ERR\n", 8);
        return;
    }
    
    if (!storage_user_exists(UID) || !storage_check_password(UID, password)) {
        send_all_tcp(client_fd, "RRI NOK\n", 8); 
        return;
    }
    
    if (!storage_is_logged_in(UID)) {
        send_all_tcp(client_fd, "RRI NLG\n", 8);
        return;
    }
    
    int res = storage_reserve(UID, EID, num_seats);
    if (res == 0) send_all_tcp(client_fd, "RRI OK\n", 7);
    else if (res == -1) send_all_tcp(client_fd, "RRI NOK\n", 8); // Event not found
    else if (res == -2) send_all_tcp(client_fd, "RRI CPT\n", 8); // Closed/Past
    else if (res == -3) send_all_tcp(client_fd, "RRI FUL\n", 8); // Full
    else send_all_tcp(client_fd, "RRI ERR\n", 8);
}

// Protocol: CPS UID old_pass new_pass
// Response: RCP OK\n | RCP NOK\n | RCP ERR\n
static void handle_cps(int client_fd, const char *buffer) {
    char UID[7], old_pass[9], new_pass[9];
    
    if (sscanf(buffer, "CPS %6s %8s %8s", UID, old_pass, new_pass) != 3) {
        send_all_tcp(client_fd, "RCP ERR\n", 8);
        return;
    }
    
    if (!validate_uid(UID) || !validate_password(old_pass) || !validate_password(new_pass)) {
        send_all_tcp(client_fd, "RCP ERR\n", 8);
        return;
    }
    
    if (!storage_user_exists(UID)) {
        send_all_tcp(client_fd, "RCP NOK\n", 8);
        return;
    }
    
    if (!storage_check_password(UID, old_pass)) {
        send_all_tcp(client_fd, "RCP NOK\n", 8);
        return;
    }
    
    if (!storage_is_logged_in(UID)) {
        send_all_tcp(client_fd, "RCP NLG\n", 8);
        return;
    }
    
    if (storage_change_password(UID, new_pass) == 0) {
        send_all_tcp(client_fd, "RCP OK\n", 7);
    } else {
        send_all_tcp(client_fd, "RCP ERR\n", 8);
    }
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
    else if (!strcmp(cmd, "LST")) handle_lst(client_fd);
    else if (!strcmp(cmd, "SED")) handle_sed(client_fd, buffer);
    else if (!strcmp(cmd, "RID")) handle_rid(client_fd, buffer);
    else if (!strcmp(cmd, "CPS")) handle_cps(client_fd, buffer);
    else send_all_tcp(client_fd, "ERR\n", 4);

    close(client_fd);
}
