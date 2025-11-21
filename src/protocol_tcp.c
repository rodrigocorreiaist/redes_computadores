#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "protocol_tcp.h"
#include "utils.h"

#define BUFFER_SIZE 1024

extern User *user_list;
extern Event *event_list;
extern Reservation *reservation_list;

static int send_all(int fd, const char *buf, size_t len) {
    while (len) {
        ssize_t n = write(fd, buf, len);
        if (n <= 0) return -1;
        buf += n; len -= n;
    }
    return 0;
}

static void handle_cre(int client_fd, const char *buffer) {
    // Placeholder implementation
    send_all(client_fd, "RCE NOK\n", 8);
}

static void handle_cls(int client_fd, const char *buffer) {
    send_all(client_fd, "RCL NOK\n", 8);
}

static void handle_lst(int client_fd, const char *buffer) {
    char response[BUFFER_SIZE];
    int count = 0;
    Event *e = event_list;
    
    // Count active events
    while (e) {
        // Assuming we list all events or just active ones? 
        // Spec says "list of available events" or "list of all events".
        // Let's list all for now, or maybe filter by status?
        // "list of currently active events" (User section)
        // "list of all events" (User section list command)
        // Let's list all.
        count++;
        e = e->next;
    }
    
    if (count == 0) {
        send_all(client_fd, "RLS NOK\n", 8);
        return;
    }
    
    send_all(client_fd, "RLS OK\n", 7);
    
    e = event_list;
    while (e) {
        // Format: EID name date time
        // We don't have time in struct, just event_date.
        // struct Event { char EID[4]; char name[11]; char event_date[20]; ... }
        snprintf(response, sizeof(response), "%s %s %s %d %d\n", 
                 e->EID, e->name, e->event_date, e->attendance_size, e->seats_reserved);
        send_all(client_fd, response, strlen(response));
        e = e->next;
    }
    send_all(client_fd, "\n", 1); // End of list marker if needed, or just close connection
}

static void handle_sed(int client_fd, const char *buffer) {
    send_all(client_fd, "RSE NOK\n", 8);
}

static void handle_rid(int client_fd, const char *buffer) {
    send_all(client_fd, "RRI NOK\n", 8);
}

static void handle_cps(int client_fd, const char *buffer) {
    char UID[7], oldPassword[9], newPassword[9];
    
    // Parse command: CPS UID oldPassword newPassword
    if (sscanf(buffer, "CPS %6s %8s %8s", UID, oldPassword, newPassword) != 3) {
        send_all(client_fd, "RCP ERR\n", 8);
        return;
    }
    
    // Validate format
    if (!validate_uid(UID) || !validate_password(oldPassword) || !validate_password(newPassword)) {
        send_all(client_fd, "RCP ERR\n", 8);
        return;
    }
    
    // Find user
    User *u = user_list;
    while (u) {
        if (strcmp(u->UID, UID) == 0) break;
        u = u->next;
    }
    
    // Check if user exists
    if (!u) {
        send_all(client_fd, "RCP NID\n", 8); // Not IDentified
        return;
    }
    
    // Check if user is logged in
    if (!u->loggedIn) {
        send_all(client_fd, "RCP NLG\n", 8); // Not Logged in
        return;
    }
    
    // Check if old password matches
    if (strcmp(u->password, oldPassword) != 0) {
        send_all(client_fd, "RCP NOK\n", 8); // Password incorrect
        return;
    }
    
    // Update password
    strcpy(u->password, newPassword);
    send_all(client_fd, "RCP OK\n", 7);
}

void process_tcp_command(int client_fd, int verbose_mode) {
    char buffer[1024];
    ssize_t n = read(client_fd, buffer, sizeof(buffer)-1);
    if (n <= 0) { close(client_fd); return; }
    buffer[n] = '\0';

    char cmd[4] = "";
    char UID[7] = "";
    if (sscanf(buffer, "%3s", cmd) != 1) {
        send_all(client_fd, "ERR\n", 4);
        close(client_fd);
        return;
    }
    
    // Extract UID for verbose logging (most TCP commands have UID as second parameter)
    sscanf(buffer + 4, "%6s", UID);
    
    if (verbose_mode) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        if (getpeername(client_fd, (struct sockaddr*)&client_addr, &addr_len) == 0) {
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            printf("[TCP] Received %s from client %s:%d (UID: %s)\n", 
                   cmd, client_ip, ntohs(client_addr.sin_port), strlen(UID) ? UID : "N/A");
        } else {
            printf("[TCP] Received %s (UID: %s)\n", cmd, strlen(UID) ? UID : "N/A");
        }
    }

    if (!strcmp(cmd, "CRE")) handle_cre(client_fd, buffer);
    else if (!strcmp(cmd, "CLS")) handle_cls(client_fd, buffer);
    else if (!strcmp(cmd, "LST")) handle_lst(client_fd, buffer);
    else if (!strcmp(cmd, "SED")) handle_sed(client_fd, buffer);
    else if (!strcmp(cmd, "RID")) handle_rid(client_fd, buffer);
    else if (!strcmp(cmd, "CPS")) handle_cps(client_fd, buffer);
    else send_all(client_fd, "ERR\n", 4);

    close(client_fd);
}
