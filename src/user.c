#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "utils.h"

#define DEFAULT_PORT "58000"
#define BUFFER_SIZE 1024

// Função para enviar comando UDP e receber resposta
int send_udp_command(int udp_fd, struct sockaddr_in *server_addr, const char *command, char *response) {
    socklen_t addr_len = sizeof(*server_addr);
    
    // Envia comando
    ssize_t sent = sendto(udp_fd, command, strlen(command), 0,
                          (struct sockaddr*)server_addr, addr_len);
    if (sent < 0) {
        perror("Erro ao enviar comando UDP");
        return -1;
    }
    
    // Recebe resposta
    ssize_t received = recvfrom(udp_fd, response, BUFFER_SIZE - 1, 0,
                                (struct sockaddr*)server_addr, &addr_len);
    if (received < 0) {
        perror("Erro ao receber resposta UDP");
        return -1;
    }
    
    response[received] = '\0';
    return 0;
}

// Comando: login
void cmd_login(int udp_fd, struct sockaddr_in *server_addr, char *logged_uid, int *logged_in) {
    char UID[10], password[20]; // Buffers maiores para detectar erros
    char command[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    
    printf("UID (6 dígitos): ");
    if (scanf("%9s", UID) != 1) {
        printf("Erro ao ler UID\n");
        return;
    }
    
    // Valida UID
    if (!validate_uid(UID)) {
        printf("Erro: UID deve ter exatamente 6 dígitos\n");
        return;
    }
    
    printf("Password (8 caracteres alfanuméricos): ");
    if (scanf("%19s", password) != 1) {
        printf("Erro ao ler password\n");
        return;
    }
    
    // Valida password
    if (!validate_password(password)) {
        printf("Erro: Password deve ter exatamente 8 caracteres alfanuméricos\n");
        return;
    }
    
    // Formata comando
    snprintf(command, BUFFER_SIZE, "LIN %s %s\n", UID, password);
    
    // Envia e recebe
    if (send_udp_command(udp_fd, server_addr, command, response) == 0) {
        show_reply(response);
        if (strncmp(response, "RLI OK", 6) == 0 || strncmp(response, "RLI REG", 7) == 0) {
            strcpy(logged_uid, UID);
            *logged_in = 1;
        }
    }
}

void cmd_logout(int udp_fd, struct sockaddr_in *server_addr, char *logged_uid, int *logged_in) {
    char password[20]; // Buffer maior para detectar erros
    char command[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    
    if (!*logged_in) {
        printf("Não está logged in\n");
        return;
    }
    
    printf("Password: ");
    if (scanf("%19s", password) != 1) {
        printf("Erro ao ler password\n");
        return;
    }
    
    // Valida password
    if (!validate_password(password)) {
        printf("Erro: Password deve ter exatamente 8 caracteres alfanuméricos\n");
        return;
    }
    
    snprintf(command, BUFFER_SIZE, "LOU %s %s\n", logged_uid, password);
    
    if (send_udp_command(udp_fd, server_addr, command, response) == 0) {
        show_reply(response);
        if (strncmp(response, "RLO OK", 6) == 0) {
            *logged_in = 0;
            memset(logged_uid, 0, 7);
        }
    }
}

void cmd_unregister(int udp_fd, struct sockaddr_in *server_addr, char *logged_uid, int *logged_in) {
    char password[20]; // Buffer maior para detectar erros
    char command[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    
    if (!*logged_in) {
        printf("Não está logged in\n");
        return;
    }
    
    printf("Password: ");
    if (scanf("%19s", password) != 1) {
        printf("Erro ao ler password\n");
        return;
    }
    
    // Valida password
    if (!validate_password(password)) {
        printf("Erro: Password deve ter exatamente 8 caracteres alfanuméricos\n");
        return;
    }
    
    snprintf(command, BUFFER_SIZE, "LUR %s %s\n", logged_uid, password);
    
    if (send_udp_command(udp_fd, server_addr, command, response) == 0) {
        show_reply(response);
        if (strncmp(response, "RUR OK", 6) == 0) {
            *logged_in = 0;
            memset(logged_uid, 0, 7);
        }
    }
}


// Menu de ajuda
void print_help() {
    printf("\nComandos disponíveis:\n");
    printf("  login         - Fazer login / registar novo utilizador\n");
    printf("  logout        - Fazer logout\n");
    printf("  unregister    - Desregistar utilizador\n");
    printf("  help          - Mostrar esta ajuda\n");
    printf("  exit          - Sair do programa\n");
    printf("\n");
}

int main(int argc, char *argv[]) {
    int udp_fd;
    struct sockaddr_in server_addr;
    char logged_uid[7] = "";
    int logged_in = 0;
    char *server_ip = "127.0.0.1";
    char *port = DEFAULT_PORT;
    char command[100];
    
    // Parse argumentos: user [-n ESIPaddress] [-p ESport]
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            server_ip = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = argv[++i];
        }
    }
    
    // Cria socket UDP
    if ((udp_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Erro ao criar socket UDP");
        return 1;
    }
    
    // Configura endereço do servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(port));
    
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Endereço IP inválido");
        close(udp_fd);
        return 1;
    }
    
    printf("Conectado ao servidor %s:%s\n", server_ip, port);
    print_help();
    
    // Loop de comandos
    while (1) {
        printf("> ");
        if (scanf("%99s", command) != 1) {
            break;
        }
        
        // Limpa buffer de entrada para evitar problemas com argumentos extras
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
        
        if (strcmp(command, "login") == 0) {
            cmd_login(udp_fd, &server_addr, logged_uid, &logged_in);
        } else if (strcmp(command, "logout") == 0) {
            cmd_logout(udp_fd, &server_addr, logged_uid, &logged_in);
        } else if (strcmp(command, "unregister") == 0) {
            cmd_unregister(udp_fd, &server_addr, logged_uid, &logged_in);
        } else if (strcmp(command, "help") == 0) {
            print_help();
        } else if (strcmp(command, "exit") == 0) {
            break;
        } else {
            printf("Comando desconhecido. Digite 'help' para ver comandos disponíveis.\n");
        }
    }
    
    // Cleanup
    close(udp_fd);
    printf("Cliente encerrado\n");
    
    return 0;
}