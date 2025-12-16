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
#define MAX_FILE_SIZE 10485760

static int recv_token_tcp(int fd, char *out, size_t out_sz) {
    if (out_sz == 0) return -1;

    size_t i = 0;
    char c;

    for (;;) {
        ssize_t n = read(fd, &c, 1);
        if (n <= 0) return -1;
        if (c != ' ' && c != '\n' && c != '\r' && c != '\t') break;
    }

    for (;;) {
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') break;
        if (i + 1 < out_sz) out[i++] = c;
        ssize_t n = read(fd, &c, 1);
        if (n <= 0) break;
    }

    out[i] = '\0';
    return 0;
}

static void handle_cre_stream(int client_fd, int verbose_mode) {
    char tok[64];
    char UID[7], password[9], name[11], date[11], time_s[6], filename[256];
    char fsize_tok[32];
    int capacity;
    size_t filesize;

    if (recv_token_tcp(client_fd, tok, sizeof(tok)) != 0 || strcmp(tok, "CRE") != 0) {
        send_all_tcp(client_fd, "RCE ERR\n", 8);
        return;
    }

    if (recv_token_tcp(client_fd, UID, sizeof(UID)) != 0 ||
        recv_token_tcp(client_fd, password, sizeof(password)) != 0 ||
        recv_token_tcp(client_fd, name, sizeof(name)) != 0 ||
        recv_token_tcp(client_fd, date, sizeof(date)) != 0 ||
        recv_token_tcp(client_fd, time_s, sizeof(time_s)) != 0 ||
        recv_token_tcp(client_fd, tok, sizeof(tok)) != 0 ||
        recv_token_tcp(client_fd, filename, sizeof(filename)) != 0 ||
        recv_token_tcp(client_fd, fsize_tok, sizeof(fsize_tok)) != 0) {
        send_all_tcp(client_fd, "RCE ERR\n", 8);
        return;
    }

    capacity = atoi(tok);
    filesize = (size_t)strtoull(fsize_tok, NULL, 10);

    if (verbose_mode) {
        printf("[TCP] CRE from UID %s: %s %s %s %d %s (%zu bytes)\n",
               UID, name, date, time_s, capacity, filename, filesize);
    }

    if (!validate_uid(UID) || !validate_password(password) ||
        !validate_event_name(name) || !validate_date(date) ||
        !validate_time(time_s) || capacity < 10 || capacity > 999 ||
        filesize == 0 || filesize > MAX_FILE_SIZE) {
        send_all_tcp(client_fd, "RCE ERR\n", 8);
        return;
    }

    if (!storage_user_exists(UID) || !storage_check_password(UID, password)) {
        send_all_tcp(client_fd, "RCE NOK\n", 8);
        return;
    }

    if (!storage_is_logged_in(UID)) {
        send_all_tcp(client_fd, "RCE NLG\n", 8);
        return;
    }

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

    char nl;
    (void)recv(client_fd, &nl, 1, MSG_DONTWAIT);

    char eid[4];
    if (storage_create_event(UID, name, date, time_s, capacity, filename, file_data, filesize, eid) != 0) {
        free(file_data);
        send_all_tcp(client_fd, "RCE NOK\n", 8);
        return;
    }

    free(file_data);
    char response[32];
    snprintf(response, sizeof(response), "RCE OK %s\n", eid);
    send_all_tcp(client_fd, response, strlen(response));
}

static void handle_cre(int client_fd, const char *buffer) {
    char UID[7], password[9], name[11], date[11], time[6], filename[256];
    int capacity;
    size_t filesize;

    int parsed = sscanf(buffer, "CRE %6s %8s %10s %10s %5s %d %255s",
                        UID, password, name, date, time, &capacity, filename);
    
    if (parsed != 7) {
        send_all_tcp(client_fd, "RCE ERR\n", 8);
        return;
    }

    char size_line[64];
    int n = recv_line_tcp(client_fd, size_line, sizeof(size_line));
    if (n <= 0) {
        send_all_tcp(client_fd, "RCE ERR\n", 8);
        return;
    }
    filesize = (size_t)strtoull(size_line, NULL, 10);

    if (!validate_uid(UID) || !validate_password(password) || 
        !validate_event_name(name) || !validate_date(date) || 
        !validate_time(time) || capacity < 10 || capacity > 999 ||
        filesize == 0 || filesize > MAX_FILE_SIZE) {
        send_all_tcp(client_fd, "RCE ERR\n", 8);
        return;
    }

    if (!storage_user_exists(UID) || !storage_check_password(UID, password)) {
        send_all_tcp(client_fd, "RCE NOK\n", 8);
        return;
    }

    if (!storage_is_logged_in(UID)) {
        send_all_tcp(client_fd, "RCE NLG\n", 8);
        return;
    }

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

    char nl;
    (void)recv(client_fd, &nl, 1, MSG_DONTWAIT);

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
    else if (res == -1) send_all_tcp(client_fd, "RCL NOE\n", 8);
    else if (res == -2) send_all_tcp(client_fd, "RCL EOW\n", 8);
    else send_all_tcp(client_fd, "RCL CLO\n", 8);
}

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
    snprintf(response, sizeof(response), "RSE OK %s %s %s %s %d %d %s %zu ",
             uid, name, date, time, attendance, reserved, fname, fsize);
             
    send_all_tcp(client_fd, response, strlen(response));
    send_all_tcp(client_fd, file_data, fsize);
    send_all_tcp(client_fd, "\n", 1);
    
    free(file_data);
}

static void handle_rid(int client_fd, const char *buffer) {
    char UID[7], password[9], EID[4];
    int num_seats;
    
    if (sscanf(buffer, "RID %6s %8s %3s %d", UID, password, EID, &num_seats) != 4) {
        send_all_tcp(client_fd, "RRI ERR\n", 8);
        return;
    }
    
    if (!validate_uid(UID) || !validate_password(password) || !validate_eid(EID) || num_seats < 1 || num_seats > 999) {
        send_all_tcp(client_fd, "RRI ERR\n", 8);
        return;
    }
    
    if (!storage_user_exists(UID)) {
        send_all_tcp(client_fd, "RRI NLG\n", 8);
        return;
    }

    if (!storage_check_password(UID, password)) {
        send_all_tcp(client_fd, "RRI WRP\n", 8);
        return;
    }
    
    if (!storage_is_logged_in(UID)) {
        send_all_tcp(client_fd, "RRI NLG\n", 8);
        return;
    }
    
    int res = storage_reserve(UID, EID, num_seats);
    if (res == 0) {
        send_all_tcp(client_fd, "RRI ACC\n", 8);
    } else if (res > 0) {
        if (res == 0) {
            send_all_tcp(client_fd, "RRI SLD\n", 8);
        } else {
            char reply[64];
            snprintf(reply, sizeof(reply), "RRI REJ %d\n", res);
            send_all_tcp(client_fd, reply, strlen(reply));
        }
    } else if (res == -2) {
        send_all_tcp(client_fd, "RRI CLS\n", 8);
    } else if (res == -3) {
        send_all_tcp(client_fd, "RRI SLD\n", 8);
    } else if (res == -5) {
        send_all_tcp(client_fd, "RRI PST\n", 8);
    } else {
        send_all_tcp(client_fd, "RRI NOK\n", 8);
    }
}

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
    /* CRE tem payload binário; não usar leitura por linhas. */
    char peek4[4];
    ssize_t pn = recv(client_fd, peek4, sizeof(peek4), MSG_PEEK);
    if (pn <= 0) {
        close(client_fd);
        return;
    }
    if (pn == 4 && memcmp(peek4, "CRE ", 4) == 0) {
        handle_cre_stream(client_fd, verbose_mode);
        close(client_fd);
        return;
    }

    char buffer[8192];

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
