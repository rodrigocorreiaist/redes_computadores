#define _POSIX_C_SOURCE 200809L

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
#include <sys/wait.h>

#define DEFAULT_PORT "58092"
#define BUFFER_SIZE 1024

int verbose_mode = 0;
volatile sig_atomic_t keep_running = 1;

void handle_signal(int sig) {
    keep_running = 0;
}

static void handle_sigchld(int sig) {
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }
}

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
    
    if (listen(tcp_fd, SOMAXCONN) < 0) {
        perror("Listen TCP failed");
        close(udp_fd); close(tcp_fd);
        exit(EXIT_FAILURE);
    }
    
    printf("Servidor iniciado na porta %s%s\n", port, verbose_mode ? " (verbose)" : "");

    signal(SIGPIPE, SIG_IGN);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; /* do not restart syscalls: we want select()/accept() to wake */
    sigaction(SIGINT, &sa, NULL);

    struct sigaction sa_chld;
    memset(&sa_chld, 0, sizeof(sa_chld));
    sa_chld.sa_handler = handle_sigchld;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa_chld, NULL);
    
    fd_set read_fds;
    int max_fd;
    
    while (keep_running) {
        FD_ZERO(&read_fds);
        FD_SET(udp_fd, &read_fds);
        FD_SET(tcp_fd, &read_fds);
        
        max_fd = (udp_fd > tcp_fd) ? udp_fd : tcp_fd;
        
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (activity < 0) {
            if (errno == EINTR) {
                if (!keep_running) break;
                continue;
            }
            perror("Select error");
            break;
        }
        
        if (activity == 0) {
            continue;
        }

        if (FD_ISSET(udp_fd, &read_fds)) {
            ssize_t n = recvfrom(udp_fd, buffer, BUFFER_SIZE - 1, 0,
                                (struct sockaddr*)&client_addr, &addr_len);
            if (n > 0) {
                process_udp_command(udp_fd, buffer, n, &client_addr, addr_len, verbose_mode);
            } else if (n < 0 && errno == EINTR && !keep_running) {
                break;
            }
        }

        if (FD_ISSET(tcp_fd, &read_fds)) {
            int new_socket = accept(tcp_fd, (struct sockaddr*)&client_addr, &addr_len);
            if (new_socket < 0) {
                if (errno == EINTR && !keep_running) break;
            } else {
                if (verbose_mode) printf("New connection accepted\n");

                pid_t pid = fork();
                if (pid < 0) {
                    perror("fork");
                    close(new_socket);
                } else if (pid == 0) {
                    /* Child process: handle this TCP connection. */
                    close(tcp_fd);
                    close(udp_fd);
                    process_tcp_command(new_socket, verbose_mode);
                    close(new_socket);
                    _exit(0);
                } else {
                    /* Parent process: close connected socket and keep accepting. */
                    close(new_socket);
                }
            }
        }
    }

    close(udp_fd);
    close(tcp_fd);
    
    return 0;
}