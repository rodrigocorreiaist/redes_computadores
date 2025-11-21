#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include "protocol_udp.h"
#include "utils.h"

extern User *user_list;
extern Event *event_list;
extern Reservation *reservation_list;
extern int verbose_mode;

static User* find_user(const char *UID) {
    User *curr = user_list; while (curr) { if (strcmp(curr->UID, UID)==0) return curr; curr = curr->next; }
    return NULL;
}


// Compute event state according to spec: 0 past,1 accepting,2 future sold out,3 closed
static int compute_event_state(const Event *e) {
    // Placeholder: without parsing date yet. Use status and seats.
    if (e->status == 3) return 3; // closed
    if (e->seats_reserved >= e->attendance_size) return 2; // sold out (future)
    // Without date logic treat as accepting (1)
    return 1;
}

void process_udp_command(int udp_fd, char *buffer, ssize_t n,
                         struct sockaddr_in *client_addr, socklen_t addr_len,
                         int verbose_mode_local) {
    char response[1024];
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
                User *u = find_user(UID);
                if (u) {
                    if (strcmp(u->password, password) == 0) {
                        u->loggedIn = 1;
                        snprintf(response, sizeof(response), "RLI OK\n");
                    } else snprintf(response, sizeof(response), "RLI NOK\n");
                } else {
                    if (register_user(UID, password) == 0) snprintf(response, sizeof(response), "RLI REG\n");
                    else snprintf(response, sizeof(response), "RLI ERR\n");
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
                int res = logout_user(UID, password);
                if (res == 0) snprintf(response, sizeof(response), "RLO OK\n");
                else if (res == -2) snprintf(response, sizeof(response), "RLO WRP\n");
                else if (res == -3) snprintf(response, sizeof(response), "RLO NOK\n");
                else snprintf(response, sizeof(response), "RLO UNR\n");
            }
        } else snprintf(response, sizeof(response), "RLO ERR\n");
    }
    else if (strcmp(command, "UNR") == 0) { // renamed from LUR
        char password[9];
        if (sscanf(buffer, "UNR %6s %8s", UID, password) == 2) {
            if (!validate_uid(UID) || !validate_password(password)) {
                snprintf(response, sizeof(response), "RUR ERR\n");
            } else {
                int res = unregister_user(UID, password);
                if (res == 0) snprintf(response, sizeof(response), "RUR OK\n");
                else if (res == -2) snprintf(response, sizeof(response), "RUR WRP\n");
                else if (res == -3) snprintf(response, sizeof(response), "RUR NOK\n");
                else snprintf(response, sizeof(response), "RUR UNR\n");
            }
        } else snprintf(response, sizeof(response), "RUR ERR\n");
    }
    else if (strcmp(command, "LME") == 0) {
        char password[9];
        if (sscanf(buffer, "LME %6s %8s", UID, password) == 2) {
            if (!validate_uid(UID) || !validate_password(password)) {
                snprintf(response, sizeof(response), "RME ERR\n");
            } else {
                User *u = find_user(UID);
                if (!u || !u->loggedIn) {
                    snprintf(response, sizeof(response), !u?"RME NLG\n":"RME NLG\n");
                } else if (strcmp(u->password, password) != 0) {
                    snprintf(response, sizeof(response), "RME WRP\n");
                } else {
                    char list[1024] = ""; int count=0; Event *e = event_list;
                    while (e) {
                        if (strcmp(e->owner_UID, UID)==0) {
                            char item[32];
                            int st = compute_event_state(e);
                            snprintf(item, sizeof(item), " %s %d", e->EID, st);
                            strncat(list, item, sizeof(list)-strlen(list)-1);
                            count++;
                        }
                        e = e->next;
                    }
                    if (!count) snprintf(response, sizeof(response), "RME NOK\n");
                    else {
                        size_t max_list = sizeof(response) - strlen("RME OK") - 2;
                        snprintf(response, sizeof(response), "RME OK%.*s\n", (int)max_list, list);
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
                User *u = find_user(UID);
                if (!u || !u->loggedIn) {
                    snprintf(response, sizeof(response), !u?"RMR NLG\n":"RMR NLG\n");
                } else if (strcmp(u->password, password) != 0) {
                    snprintf(response, sizeof(response), "RMR WRP\n");
                } else {
                    char list[1024] = ""; int count=0; Reservation *r = reservation_list;
                    while (r) {
                        if (strcmp(r->UID, UID)==0) {
                            char item[64];
                            // date currently stored as reservation_date (no separate value/time) and num_people
                            snprintf(item, sizeof(item), " %s %s %d", r->EID, r->reservation_date, r->num_people);
                            strncat(list, item, sizeof(list)-strlen(list)-1);
                            count++;
                        }
                        r = r->next;
                    }
                    if (!count) snprintf(response, sizeof(response), "RMR NOK\n");
                    else {
                        size_t max_list = sizeof(response) - strlen("RMR OK") - 2;
                        snprintf(response, sizeof(response), "RMR OK%.*s\n", (int)max_list, list);
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
