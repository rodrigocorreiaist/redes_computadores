#ifndef USER_H
#define USER_H

// Estrutura para armazenar configuração do cliente
typedef struct {
    int udp_fd;
    int tcp_fd;
    struct sockaddr_in server_addr;
    char UID[7];
    int logged_in;
} ClientState;

#endif // USER_H