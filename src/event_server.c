#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "event_server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>

#define DEFAULT_PORT "58000"
#define BUFFER_SIZE 1024

#define INITIAL_CAPACITY 10

User *users = NULL;
Event *events = NULL;
Reservation *reservations = NULL;

int user_count = 0;
int event_count = 0;
int reservation_count = 0;
int verbose_mode = 0;


void resize_users() {
    users = realloc(users, (user_count + 1) * sizeof(User));
}

void resize_events() {
    events = realloc(events, (event_count + 1) * sizeof(Event));
}

void resize_reservations() {
    reservations = realloc(reservations, (reservation_count + 1) * sizeof(Reservation));
}


int login_user(char *UID, char *password) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].UID, UID) == 0 && strcmp(users[i].password, password) == 0) {
            users[i].loggedIn = 1; // Marca como logado
            return 0; // Login bem-sucedido
        }
    }
    return -1; // Login falhou
}

int logout_user(char *UID) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].UID, UID) == 0) {
            users[i].loggedIn = 0; // Marca como não logado
            return 0; // Logout bem-sucedido
        }
    }
    return -1; // Logout falhou
}


int register_user(char *UID, char *password) {
    // Verifica se o UID já está registrado
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].UID, UID) == 0) {
            return -1; // UID já registrado
        }
    }

    // Redimensiona o array de usuários
    resize_users();

    // Adiciona novo usuário
    strcpy(users[user_count].UID, UID);
    strcpy(users[user_count].password, password);
    users[user_count].loggedIn = 0; // Inicialmente não logado
    user_count++;
    return 0; // Registro bem-sucedido
}

#pragma GCC diagnostic push // atençao, esta aqui para ignorar um erro parvo e maluco 
#pragma GCC diagnostic ignored "-Wformat-truncation"
int create_event(char *UID, char *name, char *event_date, int attendance_size) {
    if (event_count >= 999) {
        return -1; // Limite de eventos atingido
    }
    resize_events();

    // Adiciona novo evento com EID formatado "001", "002", etc.
    snprintf(events[event_count].EID, sizeof(events[event_count].EID), "%03d", event_count + 1);
    strcpy(events[event_count].name, name);
    strcpy(events[event_count].event_date, event_date);
    events[event_count].attendance_size = attendance_size;
    events[event_count].seats_reserved = 0;
    strcpy(events[event_count].owner_UID, UID);
    events[event_count].status = 0;
    event_count++;
    return 0;
}
#pragma GCC diagnostic pop

int close_event(char *UID, char *EID) {
    for (int i = 0; i < event_count; i++) {
        if (strcmp(events[i].EID, EID) == 0 && strcmp(events[i].owner_UID, UID) == 0) {
            events[i].status = 1; // Marca como fechado
            return 0; // Fechamento bem-sucedido
        }
    }
    return -1; // Fechamento falhou
}

void list_events() {
    for (int i = 0; i < event_count; i++) {
        if (events[i].status == 0) { // Apenas eventos ativos
            printf("EID: %s, Nome: %s, Data: %s, Lugares: %d, Reservados: %d\n",
                   events[i].EID, events[i].name, events[i].event_date,
                   events[i].attendance_size, events[i].seats_reserved);
        }
    }
}

int reserve_seats(char *UID, char *EID, int num_people) {
    for (int i = 0; i < event_count; i++) {
        if (strcmp(events[i].EID, EID) == 0 && events[i].status == 0) {
            if (events[i].seats_reserved + num_people <= events[i].attendance_size) {
                // Adiciona reserva
                strcpy(reservations[reservation_count].UID, UID);
                strcpy(reservations[reservation_count].EID, EID);
                reservations[reservation_count].num_people = num_people;
                // Atualiza o número de lugares reservados
                events[i].seats_reserved += num_people;
                reservation_count++;
                return 0; // Reserva bem-sucedida
            }
            return -1; // Não há lugares suficientes
        }
    }
    return -1; // Evento não encontrado ou fechado
}

void list_reservations(char *UID) {
    for (int i = 0; i < reservation_count; i++) {
        if (strcmp(reservations[i].UID, UID) == 0) {
            printf("EID: %s, Lugares Reservados: %d\n", reservations[i].EID, reservations[i].num_people);
        }
    }
}

// ...existing code...

// Função para verificar se um usuário está logado
int is_user_logged_in(char *UID) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].UID, UID) == 0) {
            return users[i].loggedIn;
        }
    }
    return 0; // Usuário não encontrado
}

// Função para listar eventos de um usuário específico
void list_my_events(char *UID) {
    int found = 0;
    for (int i = 0; i < event_count; i++) {
        if (strcmp(events[i].owner_UID, UID) == 0 && events[i].status == 0) {
            printf("EID: %s, Nome: %s, Data: %s, Lugares: %d, Reservados: %d\n",
                   events[i].EID, events[i].name, events[i].event_date,
                   events[i].attendance_size, events[i].seats_reserved);
            found = 1;
        }
    }
    if (!found) {
        printf("Nenhum evento ativo encontrado.\n");
    }
}

// Função para mostrar detalhes de um evento
int show_event(char *EID) {
    for (int i = 0; i < event_count; i++) {
        if (strcmp(events[i].EID, EID) == 0) {
            printf("EID: %s\nNome: %s\nData: %s\nLugares Totais: %d\nLugares Reservados: %d\nProprietário: %s\n",
                   events[i].EID, events[i].name, events[i].event_date,
                   events[i].attendance_size, events[i].seats_reserved, events[i].owner_UID);
            return 0;
        }
    }
    return -1; // Evento não encontrado
}

// Função para alterar senha
int change_password(char *UID, char *old_password, char *new_password) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].UID, UID) == 0 && strcmp(users[i].password, old_password) == 0) {
            strcpy(users[i].password, new_password);
            return 0; // Senha alterada com sucesso
        }
    }
    return -1; // Falha ao alterar senha
}

// Função para desregistrar usuário
int unregister_user(char *UID, char *password) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].UID, UID) == 0 && strcmp(users[i].password, password) == 0) {
            // Remove o usuário (simplificado - você pode querer reorganizar o array)
            for (int j = i; j < user_count - 1; j++) {
                users[j] = users[j + 1];
            }
            user_count--;
            return 0; // Usuário desregistrado
        }
    }
    return -1; // Falha ao desregistrar
}


// Função para processar comandos UDP
void process_udp_command(int udp_fd, char *buffer, ssize_t n, struct sockaddr_in *client_addr, socklen_t addr_len) {
    char response[BUFFER_SIZE];
    char command[4];
    char UID[7] = "";
    
    // Garante que o buffer é null-terminated
    buffer[n] = '\0';
    
    // Extrai o comando (primeiros 3 caracteres)
    sscanf(buffer, "%3s", command);
    
    // Log em verbose mode
    if (verbose_mode) {
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip, INET_ADDRSTRLEN);
        
        // Extrai UID se disponível
        sscanf(buffer + 4, "%6s", UID);
        
        printf("Received %s from %s:%d (UID: %s)\n", 
               command, client_ip, ntohs(client_addr->sin_port), 
               strlen(UID) > 0 ? UID : "N/A");
    }
    
    if (strcmp(command, "LIN") == 0) {
        // Login: LIN UID password\n
        char password[9];
        if (sscanf(buffer, "LIN %6s %8s", UID, password) == 2) {
            int result = login_user(UID, password);
            if (result == 0) {
                snprintf(response, BUFFER_SIZE, "RLI OK\n");
            } else {
                // Tenta registrar
                result = register_user(UID, password);
                if (result == 0) {
                    snprintf(response, BUFFER_SIZE, "RLI REG\n");
                } else {
                    snprintf(response, BUFFER_SIZE, "RLI NOK\n");
                }
            }
        } else {
            snprintf(response, BUFFER_SIZE, "RLI ERR\n");
        }
    }
    else if (strcmp(command, "LOU") == 0) {
        // Logout: LOU UID password\n
        char password[9];
        if (sscanf(buffer, "LOU %6s %8s", UID, password) == 2) {
            int result = logout_user(UID);
            if (result == 0) {
                snprintf(response, BUFFER_SIZE, "RLO OK\n");
            } else {
                snprintf(response, BUFFER_SIZE, "RLO NOK\n");
            }
        } else {
            snprintf(response, BUFFER_SIZE, "RLO ERR\n");
        }
    }
    else if (strcmp(command, "LUR") == 0) {
        // Unregister: LUR UID password\n
        char password[9];
        if (sscanf(buffer, "LUR %6s %8s", UID, password) == 2) {
            int result = unregister_user(UID, password);
            if (result == 0) {
                snprintf(response, BUFFER_SIZE, "RUR OK\n");
            } else {
                snprintf(response, BUFFER_SIZE, "RUR NOK\n");
            }
        } else {
            snprintf(response, BUFFER_SIZE, "RUR ERR\n");
        }
    }
    else if (strcmp(command, "LME") == 0) {
        // List My Events: LME UID\n
        if (sscanf(buffer, "LME %6s", UID) == 1) {
            // TODO: Implementar resposta completa
            snprintf(response, BUFFER_SIZE, "RME OK\n");
        } else {
            snprintf(response, BUFFER_SIZE, "RME ERR\n");
        }
    }
    else if (strcmp(command, "LEV") == 0) {
        // List Events: LEV\n
        // TODO: Implementar resposta completa
        snprintf(response, BUFFER_SIZE, "REV OK\n");
    }
    else if (strcmp(command, "LMR") == 0) {
        // List My Reservations: LMR UID\n
        if (sscanf(buffer, "LMR %6s", UID) == 1) {
            // TODO: Implementar resposta completa
            snprintf(response, BUFFER_SIZE, "RMR OK\n");
        } else {
            snprintf(response, BUFFER_SIZE, "RMR ERR\n");
        }
    }
    else {
        snprintf(response, BUFFER_SIZE, "ERR\n");
    }
    
    // Envia resposta
    sendto(udp_fd, response, strlen(response), 0, 
           (struct sockaddr*)client_addr, addr_len);
}



// Função para processar comandos TCP
void process_tcp_command(int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t n = read(client_fd, buffer, BUFFER_SIZE - 1);
    
    if (n <= 0) {
        close(client_fd);
        return;
    }
    
    buffer[n] = '\0';
    char command[4];
    sscanf(buffer, "%3s", command);
    
    // TODO: Implementar comandos TCP (CRE, RID, etc.)
    char response[BUFFER_SIZE];
    snprintf(response, BUFFER_SIZE, "ERR\n");
    write(client_fd, response, strlen(response));
    
    close(client_fd);
}

int main(int argc, char *argv[]) {
    int udp_fd, tcp_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    char *port = DEFAULT_PORT;
    
    // Parse argumentos
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = argv[++i];
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose_mode = 1;
        }
    }
    
    // Inicializa os arrays globais
    users = malloc(INITIAL_CAPACITY * sizeof(User));
    events = malloc(INITIAL_CAPACITY * sizeof(Event));
    reservations = malloc(INITIAL_CAPACITY * sizeof(Reservation));
    if (!users || !events || !reservations) {
        fprintf(stderr, "Falha ao alocar memória inicial\n");
        return 1;
    }
    
    // Cria socket UDP
    if ((udp_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Erro ao criar socket UDP");
        return 1;
    }
    
    // Cria socket TCP
    if ((tcp_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Erro ao criar socket TCP");
        close(udp_fd);
        return 1;
    }
    
    // Configura endereço do servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(atoi(port));
    
    // Bind UDP
    if (bind(udp_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erro no bind UDP");
        close(udp_fd);
        close(tcp_fd);
        return 1;
    }
    
    // Bind TCP
    if (bind(tcp_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erro no bind TCP");
        close(udp_fd);
        close(tcp_fd);
        return 1;
    }
    
    // Listen TCP
    if (listen(tcp_fd, 5) < 0) {
        perror("Erro no listen TCP");
        close(udp_fd);
        close(tcp_fd);
        return 1;
    }
    
    printf("Servidor iniciado na porta %s\n", port);
    
    // Loop principal com select()
    fd_set read_fds;
    int max_fd = (udp_fd > tcp_fd) ? udp_fd : tcp_fd;
    
    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(udp_fd, &read_fds);
        FD_SET(tcp_fd, &read_fds);
        
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        
        if (activity < 0 && errno != EINTR) {
            perror("Erro no select");
            break;
        }
        
        // Verifica UDP
        if (FD_ISSET(udp_fd, &read_fds)) {
            ssize_t n = recvfrom(udp_fd, buffer, BUFFER_SIZE - 1, 0,
                                (struct sockaddr*)&client_addr, &addr_len);
            if (n > 0) {
                process_udp_command(udp_fd, buffer, n, &client_addr, addr_len);
            }
        }
        
        // Verifica TCP
        if (FD_ISSET(tcp_fd, &read_fds)) {
            int client_fd = accept(tcp_fd, (struct sockaddr*)&client_addr, &addr_len);
            if (client_fd >= 0) {
                process_tcp_command(client_fd);
            }
        }
    }
    
    // Cleanup
    close(udp_fd);
    close(tcp_fd);
    free(users);
    free(events);
    free(reservations);
    
    return 0;
}