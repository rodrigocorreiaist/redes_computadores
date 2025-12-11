#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "event_server.h"
#include "utils.h"
#include "protocol_udp.h"
#include "protocol_tcp.h"
#include "storage.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>
#include <signal.h>

#define DEFAULT_PORT "58092"
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 30

int verbose_mode = 0;
volatile sig_atomic_t keep_running = 1;

void handle_signal(int sig) {
    keep_running = 0;
}

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

    if (storage_init() != 0) {
        fprintf(stderr, "Failed to initialize storage\n");
        exit(EXIT_FAILURE);
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
    
    // Ignore SIGPIPE to prevent crash on client disconnect
    signal(SIGPIPE, SIG_IGN);
    // Handle SIGINT for graceful shutdown
    signal(SIGINT, handle_signal);
    
    fd_set read_fds;
    int max_fd;
    int client_socket[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) client_socket[i] = 0;
    
    while (keep_running) {
        FD_ZERO(&read_fds);
        FD_SET(udp_fd, &read_fds);
        FD_SET(tcp_fd, &read_fds);
        
        max_fd = (udp_fd > tcp_fd) ? udp_fd : tcp_fd;
        
        // Add child sockets to set
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_socket[i];
            if (sd > 0) FD_SET(sd, &read_fds);
            if (sd > max_fd) max_fd = sd;
        }
        
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (activity < 0 && errno != EINTR) {
            perror("Select error");
            break;
        }
        
        if (activity == 0) {
            // Timeout event - can be used for maintenance tasks
            continue;
        }
        
        // Handle UDP
        if (FD_ISSET(udp_fd, &read_fds)) {
            ssize_t n = recvfrom(udp_fd, buffer, BUFFER_SIZE - 1, 0,
                                (struct sockaddr*)&client_addr, &addr_len);
            if (n > 0) process_udp_command(udp_fd, buffer, n, &client_addr, addr_len, verbose_mode);
        }
        
        // Handle New TCP Connections
        if (FD_ISSET(tcp_fd, &read_fds)) {
            int new_socket = accept(tcp_fd, (struct sockaddr*)&client_addr, &addr_len);
            if (new_socket >= 0) {
                if (verbose_mode) printf("New connection accepted\n");
                
                // Add to array
                int added = 0;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (client_socket[i] == 0) {
                        client_socket[i] = new_socket;
                        added = 1;
                        break;
                    }
                }
                
                if (!added) {
                    if (verbose_mode) printf("Too many clients, rejecting\n");
                    close(new_socket);
                }
            }
        }
        
        // Handle IO on TCP Clients
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_socket[i];
            if (sd > 0 && FD_ISSET(sd, &read_fds)) {
                // Process command (blocking for now, but allows other clients to connect first)
                process_tcp_command(sd, verbose_mode);
                
                // Socket is closed inside process_tcp_command, so we just clear the slot
                client_socket[i] = 0;
            }
        }
    }
    
    close(udp_fd);
    close(tcp_fd);
    
    return 0;
}