#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "event_server.h"
#include "utils.h"
#include "protocol_udp.h"
#include "protocol_tcp.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>

#define DEFAULT_PORT "58092"
#define BUFFER_SIZE 1024

// Inicialização das listas globais
User *user_list = NULL;
Event *event_list = NULL;
Reservation *reservation_list = NULL;

int total_events = 0; // Contador para gerar EIDs únicos
int verbose_mode = 0;

// --- Funções Auxiliares ---

User* find_user(const char *UID) {
    User *curr = user_list;
    while (curr) {
        if (strcmp(curr->UID, UID) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

Event* find_event(const char *EID) {
    Event *curr = event_list;
    while (curr) {
        if (strcmp(curr->EID, EID) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

// --- Gestão de Utilizadores ---

int register_user(char *UID, char *password) {
    if (find_user(UID)) return -1; // Já existe

    User *new_user = (User *)malloc(sizeof(User));
    if (!new_user) return -1;

    strcpy(new_user->UID, UID);
    strcpy(new_user->password, password);
    new_user->loggedIn = 1; // Auto-login
    
    new_user->next = user_list;
    user_list = new_user;

    return 0;
}

int login_user(char *UID, char *password) {
    User *u = find_user(UID);
    if (!u) return -1;
    
    if (strcmp(u->password, password) == 0) {
        u->loggedIn = 1;
        return 0;
    }
    return -1;
}

int logout_user(char *UID, char *password) {
    User *u = find_user(UID);
    if (!u) return -1; // UNR

    if (strcmp(u->password, password) != 0) return -2; // WRP
    if (!u->loggedIn) return -3; // NOK
    
    u->loggedIn = 0;
    return 0;
}

int unregister_user(char *UID, char *password) {
    User *curr = user_list;
    User *prev = NULL;

    while (curr != NULL) {
        if (strcmp(curr->UID, UID) == 0) {
            if (strcmp(curr->password, password) != 0) return -2; // WRP
            if (!curr->loggedIn) return -3; // NOK
            
            if (prev == NULL) user_list = curr->next;
            else prev->next = curr->next;
            
            free(curr);
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }
    return -1; // UNR
}

// --- Gestão de Eventos e Reservas (Skeleton para TCP/UDP) ---

int create_event(char *UID, char *name, char *event_date, int attendance_size) {
    if (total_events >= 999) return -1;

    Event *new_event = (Event *)malloc(sizeof(Event));
    if (!new_event) return -1;

    total_events++;
    snprintf(new_event->EID, sizeof(new_event->EID), "%03d", total_events);
    strcpy(new_event->name, name);
    strcpy(new_event->event_date, event_date);
    new_event->attendance_size = attendance_size;
    new_event->seats_reserved = 0;
    strcpy(new_event->owner_UID, UID);
    new_event->status = 0; // 0: ativo

    new_event->next = event_list;
    event_list = new_event;
    return 0;
}

// Removed inline protocol handlers; now using protocol_udp.c and protocol_tcp.c

// --- Main ---

int main(int argc, char *argv[]) {
    int udp_fd, tcp_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    char *port = DEFAULT_PORT;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = argv[++i];
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose_mode = 1;
        }
    }
    
    if ((udp_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket UDP failed");
        exit(EXIT_FAILURE);
    }
    
    if ((tcp_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket TCP failed");
        close(udp_fd);
        exit(EXIT_FAILURE);
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(atoi(port));
    
    if (bind(udp_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind UDP failed");
        close(udp_fd); close(tcp_fd);
        exit(EXIT_FAILURE);
    }
    
    if (bind(tcp_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind TCP failed");
        close(udp_fd); close(tcp_fd);
        exit(EXIT_FAILURE);
    }
    
    if (listen(tcp_fd, 5) < 0) {
        perror("Listen TCP failed");
        close(udp_fd); close(tcp_fd);
        exit(EXIT_FAILURE);
    }
    
    printf("Servidor iniciado na porta %s%s\n", port, verbose_mode ? " (verbose)" : "");
    
    fd_set read_fds;
    int max_fd = (udp_fd > tcp_fd) ? udp_fd : tcp_fd;
    
    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(udp_fd, &read_fds);
        FD_SET(tcp_fd, &read_fds);
        
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0 && errno != EINTR) {
            perror("Select error");
            break;
        }
        
        if (FD_ISSET(udp_fd, &read_fds)) {
            ssize_t n = recvfrom(udp_fd, buffer, BUFFER_SIZE - 1, 0,
                                (struct sockaddr*)&client_addr, &addr_len);
            if (n > 0) process_udp_command(udp_fd, buffer, n, &client_addr, addr_len, verbose_mode);
        }
        
        if (FD_ISSET(tcp_fd, &read_fds)) {
            int client_fd = accept(tcp_fd, (struct sockaddr*)&client_addr, &addr_len);
            if (client_fd >= 0) process_tcp_command(client_fd, verbose_mode);
        }
    }
    
    close(udp_fd);
    close(tcp_fd);
    
    // Cleanup lists
    while (user_list) { User *t = user_list; user_list = user_list->next; free(t); }
    while (event_list) { Event *t = event_list; event_list = event_list->next; free(t); }
    while (reservation_list) { Reservation *t = reservation_list; reservation_list = reservation_list->next; free(t); }
    
    return 0;
}