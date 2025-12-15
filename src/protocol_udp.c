#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include "protocol_udp.h"
#include "utils.h"
#include "storage.h"

extern int verbose_mode;

void process_udp_command(int udp_fd, char *buffer, ssize_t n,
                         struct sockaddr_in *client_addr, socklen_t addr_len,
                         int verbose_mode_local) {
    char response[65535];
    char command[4] = "";
    char UID[7] = "";
    buffer[n] = '\0';
    sscanf(buffer, "%3s", command);

    if (verbose_mode_local) {
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip, INET_ADDRSTRLEN);
        sscanf(buffer + 4, "%6s", UID);
        printf("[UDP] Received %s from client %s:%d (UID: %s)\n", command, client_ip, ntohs(client_addr->sin_port), strlen(UID)?UID:"N/A");
    }

    if (strcmp(command, "LIN") == 0) {
        char password[9];
        if (sscanf(buffer, "LIN %6s %8s", UID, password) == 2) {
            if (!validate_uid(UID) || !validate_password(password)) {
                snprintf(response, sizeof(response), "RLI ERR\n");
            } else {
                if (storage_user_exists(UID)) {
                    if (storage_check_password(UID, password)) {
                        storage_login_user(UID);
                        snprintf(response, sizeof(response), "RLI OK\n");
                    } else {
                        snprintf(response, sizeof(response), "RLI NOK\n");
                    }
                } else {
                    if (storage_register_user(UID, password) == 0) {
                        storage_login_user(UID);
                        snprintf(response, sizeof(response), "RLI REG\n");
                    } else {
                        snprintf(response, sizeof(response), "RLI ERR\n");
                    }
                }
            }
        } else snprintf(response, sizeof(response), "RLI ERR\n");
    }
    else if (strcmp(command, "LOU") == 0) {
        char password[9];
        if (sscanf(buffer, "LOU %6s %8s", UID, password) == 2) {
            if (!validate_uid(UID) || !validate_password(password)) {
                snprintf(response, sizeof(response), "RLO ERR\n");
            } else {
                if (!storage_user_exists(UID)) {
                    snprintf(response, sizeof(response), "RLO UNR\n");
                } else if (!storage_check_password(UID, password)) {
                    snprintf(response, sizeof(response), "RLO WRP\n");
                } else if (!storage_is_logged_in(UID)) {
                    snprintf(response, sizeof(response), "RLO NOK\n");
                } else {
                    storage_logout_user(UID);
                    snprintf(response, sizeof(response), "RLO OK\n");
                }
            }
        } else snprintf(response, sizeof(response), "RLO ERR\n");
    }
    else if (strcmp(command, "UNR") == 0) {
        char password[9];
        if (sscanf(buffer, "UNR %6s %8s", UID, password) == 2) {
            if (!validate_uid(UID) || !validate_password(password)) {
                snprintf(response, sizeof(response), "RUR ERR\n");
            } else {
                if (!storage_user_exists(UID)) {
                    snprintf(response, sizeof(response), "RUR UNR\n");
                } else if (!storage_check_password(UID, password)) {
                    snprintf(response, sizeof(response), "RUR WRP\n");
                } else if (!storage_is_logged_in(UID)) {
                    snprintf(response, sizeof(response), "RUR NOK\n");
                } else {
                    storage_unregister_user(UID);
                    snprintf(response, sizeof(response), "RUR OK\n");
                }
            }
        } else snprintf(response, sizeof(response), "RUR ERR\n");
    }
    else if (strcmp(command, "LME") == 0) {
        char password[9];
        if (sscanf(buffer, "LME %6s %8s", UID, password) == 2) {
            if (!validate_uid(UID) || !validate_password(password)) {
                snprintf(response, sizeof(response), "RME ERR\n");
            } else {
                if (!storage_user_exists(UID)) {
                    snprintf(response, sizeof(response), "RME NLG\n");
                } else if (!storage_check_password(UID, password)) {
                    snprintf(response, sizeof(response), "RME WRP\n");
                } else if (!storage_is_logged_in(UID)) {
                    snprintf(response, sizeof(response), "RME NLG\n");
                } else {
                    char list_buffer[60000];
                    storage_list_my_events(UID, list_buffer, sizeof(list_buffer));
                    if (strlen(list_buffer) == 0) {
                        snprintf(response, sizeof(response), "RME NOK\n");
                    } else {
                        snprintf(response, sizeof(response), "RME OK%s\n", list_buffer);
                    }
                }
            }
        } else snprintf(response, sizeof(response), "RME ERR\n");
    }
    else if (strcmp(command, "LMR") == 0) {
        char password[9];
        if (sscanf(buffer, "LMR %6s %8s", UID, password) == 2) {
            if (!validate_uid(UID) || !validate_password(password)) {
                snprintf(response, sizeof(response), "RMR ERR\n");
            } else {
                if (!storage_user_exists(UID)) {
                    snprintf(response, sizeof(response), "RMR NLG\n");
                } else if (!storage_check_password(UID, password)) {
                    snprintf(response, sizeof(response), "RMR WRP\n");
                } else if (!storage_is_logged_in(UID)) {
                    snprintf(response, sizeof(response), "RMR NLG\n");
                } else {
                    char list_buffer[60000];
                    storage_list_my_reservations(UID, list_buffer, sizeof(list_buffer));
                    if (strlen(list_buffer) == 0) {
                        snprintf(response, sizeof(response), "RMR NOK\n");
                    } else {
                        snprintf(response, sizeof(response), "RMR OK%s\n", list_buffer);
                    }
                }
            }
        } else snprintf(response, sizeof(response), "RMR ERR\n");
    }
    else {
        snprintf(response, sizeof(response), "ERR\n");
    }

    sendto(udp_fd, response, strlen(response), 0, (struct sockaddr*)client_addr, addr_len);
}

