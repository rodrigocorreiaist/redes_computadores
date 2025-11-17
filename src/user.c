#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define DEFAULT_PORT "58000"
#define BUFFER_SIZE 1024

// Estrutura para armazenar configuração do cliente
typedef struct {
    int udp_fd;
    int tcp_fd;
    struct sockaddr_in server_addr;
    char UID[7];
    int logged_in;
} ClientState;

// Função para enviar comando UDP e receber resposta
int send_udp_command(ClientState *state, const char *command, char *response) {
    socklen_t addr_len = sizeof(state->server_addr);
    
    // Envia comando
    ssize_t sent = sendto(state->udp_fd, command, strlen(command), 0,
                          (struct sockaddr*)&state->server_addr, addr_len);
    if (sent < 0) {
        perror("Erro ao enviar comando UDP");
        return -1;
    }
    
    // Recebe resposta
    ssize_t received = recvfrom(state->udp_fd, response, BUFFER_SIZE - 1, 0,
                                (struct sockaddr*)&state->server_addr, &addr_len);
    if (received < 0) {
        perror("Erro ao receber resposta UDP");
        return -1;
    }
    
    response[received] = '\0';
    return 0;
}

// Comando: login
void cmd_login(ClientState *state) {
    char UID[7], password[9];
    char command[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    
    printf("UID (6 dígitos): ");
    if (scanf("%6s", UID) != 1) {
        printf("Erro ao ler UID\n");
        return;
    }
    
    printf("Password (8 caracteres alfanuméricos): ");
    if (scanf("%8s", password) != 1) {
        printf("Erro ao ler password\n");
        return;
    }
    
    // Formata comando
    snprintf(command, BUFFER_SIZE, "LIN %s %s\n", UID, password);
    
    // Envia e recebe
    if (send_udp_command(state, command, response) == 0) {
        if (strncmp(response, "RLI OK", 6) == 0) {
            printf("Login bem-sucedido\n");
            strcpy(state->UID, UID);
            state->logged_in = 1;
        } else if (strncmp(response, "RLI REG", 7) == 0) {
            printf("Novo utilizador registado\n");
            strcpy(state->UID, UID);
            state->logged_in = 1;
        } else if (strncmp(response, "RLI NOK", 7) == 0) {
            printf("Login falhou: credenciais incorretas\n");
        } else {
            printf("Erro no login: %s", response);
        }
    }
}

// Comando: logout
void cmd_logout(ClientState *state) {
    char password[9];
    char command[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    
    if (!state->logged_in) {
        printf("Não está logged in\n");
        return;
    }
    
    printf("Password: ");
    if (scanf("%8s", password) != 1) {
        printf("Erro ao ler password\n");
        return;
    }
    
    snprintf(command, BUFFER_SIZE, "LOU %s %s\n", state->UID, password);
    
    if (send_udp_command(state, command, response) == 0) {
        if (strncmp(response, "RLO OK", 6) == 0) {
            printf("Logout bem-sucedido\n");
            state->logged_in = 0;
            memset(state->UID, 0, sizeof(state->UID));
        } else {
            printf("Logout falhou: %s", response);
        }
    }
}

// Comando: unregister
void cmd_unregister(ClientState *state) {
    char password[9];
    char command[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    
    if (!state->logged_in) {
        printf("Não está logged in\n");
        return;
    }
    
    printf("Password: ");
    if (scanf("%8s", password) != 1) {
        printf("Erro ao ler password\n");
        return;
    }
    
    snprintf(command, BUFFER_SIZE, "LUR %s %s\n", state->UID, password);  // Corrigido: UNR -> LUR
    
    if (send_udp_command(state, command, response) == 0) {
        if (strncmp(response, "RUR OK", 6) == 0) {
            printf("Utilizador desregistado\n");
            state->logged_in = 0;
            memset(state->UID, 0, sizeof(state->UID));
        } else {
            printf("Desregistro falhou: %s", response);
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
    ClientState state;
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
    
    // Inicializa estado
    memset(&state, 0, sizeof(state));
    state.logged_in = 0;
    
    // Cria socket UDP
    if ((state.udp_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Erro ao criar socket UDP");
        return 1;
    }
    
    // Configura endereço do servidor
    memset(&state.server_addr, 0, sizeof(state.server_addr));
    state.server_addr.sin_family = AF_INET;
    state.server_addr.sin_port = htons(atoi(port));
    
    if (inet_pton(AF_INET, server_ip, &state.server_addr.sin_addr) <= 0) {
        perror("Endereço IP inválido");
        close(state.udp_fd);
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
            cmd_login(&state);
        } else if (strcmp(command, "logout") == 0) {
            cmd_logout(&state);
        } else if (strcmp(command, "unregister") == 0) {
            cmd_unregister(&state);
        } else if (strcmp(command, "help") == 0) {
            print_help();
        } else if (strcmp(command, "exit") == 0) {
            break;
        } else {
            printf("Comando desconhecido. Digite 'help' para ver comandos disponíveis.\n");
        }
    }
    
    // Cleanup
    close(state.udp_fd);
    printf("Cliente encerrado\n");
    
    return 0;
}