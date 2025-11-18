#ifndef USER_H
#define USER_H

// Estrutura para armazenar estado do cliente
typedef struct {
    char UID[7];
    int logged_in;
} ClientState;

// Estrutura para armazenar configuração do cliente
typedef struct {
    int udp_fd;
    int tcp_fd;
    struct sockaddr_in server_addr;
    ClientState state;
} Client;

#endif // USER_H