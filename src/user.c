#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include "utils.h"

#define DEFAULT_PORT "58092"
#define BUFFER_SIZE 1024

static volatile sig_atomic_t keep_running = 1;

static void handle_sigint(int sig) {
    (void)sig;
    keep_running = 0;
}

static int recv_token_tcp(int fd, char *out, size_t out_sz) {
    if (out_sz == 0) return -1;

    size_t i = 0;
    char c;

    for (;;) {
        ssize_t n = read(fd, &c, 1);
        if (n <= 0) return -1;
        if (c != ' ' && c != '\n' && c != '\r' && c != '\t') break;
    }

    for (;;) {
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') break;
        if (i + 1 < out_sz) out[i++] = c;
        ssize_t n = read(fd, &c, 1);
        if (n <= 0) break;
    }

    out[i] = '\0';
    return 0;
}

int send_udp_command(int udp_fd, struct sockaddr_in *server_addr, const char *command, char *response) {
    socklen_t addr_len = sizeof(*server_addr);

    ssize_t sent = sendto(udp_fd, command, strlen(command), 0,
                          (struct sockaddr*)server_addr, addr_len);
    if (sent < 0) {
        if (errno == EINTR && !keep_running) return -1;
        perror("Erro ao enviar comando UDP");
        return -1;
    }

    ssize_t received = recvfrom(udp_fd, response, BUFFER_SIZE - 1, 0,
                                (struct sockaddr*)server_addr, &addr_len);
    if (received < 0) {
        if (errno == EINTR && !keep_running) return -1;
        perror("Erro ao receber resposta UDP");
        return -1;
    }
    
    response[received] = '\0';
    return 0;
}

void cmd_login(int udp_fd, struct sockaddr_in *server_addr, char *logged_uid, char *logged_pass, int *logged_in, char *args) {
    char UID[10], password[20];
    char command[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    
    if (sscanf(args, "%9s %19s", UID, password) != 2) {
        printf("Uso: login <UID> <password>\n");
        return;
    }
    
    if (!validate_uid(UID)) {
        printf("Erro: UID deve ter exatamente 6 dígitos\n");
        return;
    }

    if (!validate_password(password)) {
        printf("Erro: Password deve ter exatamente 8 caracteres alfanuméricos\n");
        return;
    }

    snprintf(command, BUFFER_SIZE, "LIN %s %s\n", UID, password);

    if (send_udp_command(udp_fd, server_addr, command, response) == 0) {
        show_reply(response);
        if (strncmp(response, "RLI OK", 6) == 0 || strncmp(response, "RLI REG", 7) == 0) {
            strcpy(logged_uid, UID);
            strcpy(logged_pass, password);
            *logged_in = 1;
        }
    }
}

void cmd_logout(int udp_fd, struct sockaddr_in *server_addr, char *logged_uid, char *logged_pass, int *logged_in, char *args) {
    char command[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    
    if (!*logged_in) {
        printf("Não está logged in\n");
        return;
    }
    
    snprintf(command, BUFFER_SIZE, "LOU %s %s\n", logged_uid, logged_pass);
    
    if (send_udp_command(udp_fd, server_addr, command, response) == 0) {
        show_reply(response);
        if (strncmp(response, "RLO OK", 6) == 0) {
            *logged_in = 0;
            memset(logged_uid, 0, 7);
            memset(logged_pass, 0, 20);
        }
    }
}

void cmd_unregister(int udp_fd, struct sockaddr_in *server_addr, char *logged_uid, char *logged_pass, int *logged_in, char *args) {
    char password[20];
    char command[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    
    if (!*logged_in) {
        printf("Não está logged in\n");
        return;
    }
    
    if (sscanf(args, "%19s", password) != 1) {
        printf("Uso: unregister <password>\n");
        return;
    }
    
    if (!validate_password(password)) {
        printf("Erro: Password deve ter exatamente 8 caracteres alfanuméricos\n");
        return;
    }
    
    snprintf(command, BUFFER_SIZE, "UNR %s %s\n", logged_uid, password);
    
    if (send_udp_command(udp_fd, server_addr, command, response) == 0) {
        show_reply(response);
        if (strncmp(response, "RUR OK", 6) == 0) {
            *logged_in = 0;
            memset(logged_uid, 0, 7);
            memset(logged_pass, 0, 20);
        }
    }
}

void cmd_myevents(int udp_fd, struct sockaddr_in *server_addr, char *logged_uid, char *logged_pass, int *logged_in, char *args) {
    char command[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    
    if (!*logged_in) {
        printf("Não está logged in\n");
        return;
    }
    
    snprintf(command, BUFFER_SIZE, "LME %s %s\n", logged_uid, logged_pass);
    
    if (send_udp_command(udp_fd, server_addr, command, response) == 0) {
        show_reply(response);
    }
}

void cmd_myreservations(int udp_fd, struct sockaddr_in *server_addr, char *logged_uid, char *logged_pass, int *logged_in, char *args) {
    char command[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    
    if (!*logged_in) {
        printf("Não está logged in\n");
        return;
    }
    
    snprintf(command, BUFFER_SIZE, "LMR %s %s\n", logged_uid, logged_pass);
    
    if (send_udp_command(udp_fd, server_addr, command, response) == 0) {
        show_reply(response);
    }
}

void cmd_changepass(struct sockaddr_in *server_addr, char *logged_uid, char *logged_pass, int *logged_in, char *args) {
    char oldPassword[20], newPassword[20];
    char command[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    int tcp_fd;
    
    if (!*logged_in) {
        printf("Não está logged in\n");
        return;
    }
    
    if (sscanf(args, "%19s %19s", oldPassword, newPassword) != 2) {
        printf("Uso: changepass <old_password> <new_password>\n");
        return;
    }
    
    if (!validate_password(oldPassword)) {
        printf("Erro: Password deve ter exatamente 8 caracteres alfanuméricos\n");
        return;
    }
    
    if (!validate_password(newPassword)) {
        printf("Erro: Nova password deve ter exatamente 8 caracteres alfanuméricos\n");
        return;
    }
    
    if ((tcp_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Erro ao criar socket TCP");
        return;
    }

    if (connect(tcp_fd, (struct sockaddr*)server_addr, sizeof(*server_addr)) < 0) {
        perror("Erro ao conectar ao servidor TCP");
        close(tcp_fd);
        return;
    }

    snprintf(command, BUFFER_SIZE, "CPS %s %s %s\n", logged_uid, oldPassword, newPassword);
    ssize_t sent = write(tcp_fd, command, strlen(command));
    if (sent < 0) {
        perror("Erro ao enviar comando TCP");
        close(tcp_fd);
        return;
    }

    ssize_t received = read(tcp_fd, response, BUFFER_SIZE - 1);
    if (received < 0) {
        perror("Erro ao receber resposta TCP");
        close(tcp_fd);
        return;
    }
    
    response[received] = '\0';
    close(tcp_fd);

    if (strncmp(response, "RCP OK", 6) == 0) {
        printf("Password alterada com sucesso\n");
        strcpy(logged_pass, newPassword);
    } else if (strncmp(response, "RCP NOK", 7) == 0) {
        printf("ChangePass: password atual incorreta\n");
    } else if (strncmp(response, "RCP NLG", 7) == 0) {
        printf("ChangePass: utilizador não está autenticado\n");
    } else if (strncmp(response, "RCP NID", 7) == 0) {
        printf("ChangePass: utilizador não existe\n");
    } else if (strncmp(response, "RCP ERR", 7) == 0) {
        printf("ChangePass: erro de formato\n");
    } else {
        printf("ChangePass: resposta desconhecida (%s)\n", response);
    }
}

void cmd_list(struct sockaddr_in *server_addr, char *logged_uid, int *logged_in, char *args) {
    char command[BUFFER_SIZE];
    char buf[BUFFER_SIZE];
    int tcp_fd;
    ssize_t n;
    
    if ((tcp_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Erro ao criar socket TCP");
        return;
    }

    if (connect(tcp_fd, (struct sockaddr*)server_addr, sizeof(*server_addr)) < 0) {
        perror("Erro ao conectar ao servidor TCP");
        close(tcp_fd);
        return;
    }

    snprintf(command, BUFFER_SIZE, "LST\n");

    ssize_t sent = write(tcp_fd, command, strlen(command));
    if (sent < 0) {
        perror("Erro ao enviar comando TCP");
        close(tcp_fd);
        return;
    }
    
    n = read(tcp_fd, buf, sizeof(buf) - 1);
    if (n < 0) {
        perror("Erro ao receber resposta TCP");
        close(tcp_fd);
        return;
    }
    
    if (n == 0) {
        close(tcp_fd);
        return;
    }
    
    buf[n] = '\0';
    
    if (strncmp(buf, "RLS NOK", 7) == 0) {
        printf("List: nenhum evento disponível\n");
    } else {
        printf("%s", buf);
        while ((n = read(tcp_fd, buf, sizeof(buf) - 1)) > 0) {
            buf[n] = '\0';
            printf("%s", buf);
        }
    }
    
    close(tcp_fd);
}

void cmd_create(struct sockaddr_in *server_addr, char *logged_uid, char *logged_pass, int *logged_in, char *args) {
    char name[12], date[12], time[7], filename[256];
    int capacity;
    size_t filesize;
    int tcp_fd;
    
    if (!*logged_in) {
        printf("Não está logged in\n");
        return;
    }

    if (sscanf(args, "%11s %255s %11s %6s %d", name, filename, date, time, &capacity) != 5) {
        printf("Uso: create <name> <event_fname> <event_date> <num_attendees>\n");
        printf("  event_date: DD-MM-YYYY HH:MM\n");
        return;
    }
    
    if (!validate_event_name(name)) {
        printf("Erro: Nome inválido (%s)\n", name);
        return;
    }
    
    if (!validate_date(date)) {
        printf("Erro: Data inválida (%s)\n", date);
        return;
    }
    
    if (!validate_time(time)) {
        printf("Erro: Hora inválida (%s)\n", time);
        return;
    }
    
    if (capacity < 10 || capacity > 999) {
        printf("Erro: Capacidade inválida\n");
        return;
    }
    
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        printf("Erro: Não foi possível abrir ficheiro\n");
        return;
    }
    
    fseek(fp, 0, SEEK_END);
    filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (filesize == 0 || filesize > 10485760) {
        printf("Erro: Tamanho de ficheiro inválido\n");
        fclose(fp);
        return;
    }
    
    char *file_data = (char *)malloc(filesize);
    if (!file_data) {
        printf("Erro: Memória insuficiente\n");
        fclose(fp);
        return;
    }
    
    if (fread(file_data, 1, filesize, fp) != filesize) {
        printf("Erro: Falha ao ler ficheiro\n");
        free(file_data);
        fclose(fp);
        return;
    }
    fclose(fp);
    
    char *basename = strrchr(filename, '/');
    basename = basename ? basename + 1 : filename;

    if ((tcp_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Erro ao criar socket TCP");
        free(file_data);
        return;
    }
    
    if (connect(tcp_fd, (struct sockaddr*)server_addr, sizeof(*server_addr)) < 0) {
        perror("Erro ao conectar ao servidor TCP");
        close(tcp_fd);
        free(file_data);
        return;
    }

    char command[512];
    snprintf(command, sizeof(command), "CRE %s %s %s %s %s %d %s %zu ",
             logged_uid, logged_pass, name, date, time, capacity, basename, filesize);
    if (send_all_tcp(tcp_fd, command, strlen(command)) < 0) {
        printf("Erro ao enviar comando\n");
        close(tcp_fd);
        free(file_data);
        return;
    }

    if (send_all_tcp(tcp_fd, file_data, filesize) < 0) {
        printf("Erro ao enviar ficheiro\n");
        close(tcp_fd);
        free(file_data);
        return;
    }

    if (send_all_tcp(tcp_fd, "\n", 1) < 0) {
        printf("Erro ao enviar terminador\n");
        close(tcp_fd);
        free(file_data);
        return;
    }
    free(file_data);

    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    setsockopt(tcp_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    char response[128];
    int n = recv_line_tcp(tcp_fd, response, sizeof(response));
    close(tcp_fd);
    
    if (n > 0) {
        if (strncmp(response, "RCE OK", 6) == 0) {
            char eid[4];
            sscanf(response, "RCE OK %3s", eid);
            printf("Evento criado com sucesso. EID: %s\n", eid);
        } else if (strncmp(response, "RCE NOK", 7) == 0) {
            printf("Create: falha ao criar evento\n");
        } else if (strncmp(response, "RCE NLG", 7) == 0) {
            printf("Create: não autenticado\n");
        } else if (strncmp(response, "RCE ERR", 7) == 0) {
            printf("Create: erro de formato\n");
        } else {
            printf("Create: resposta desconhecida (%s)\n", response);
        }
    } else if (n == -2) {
        printf("Create: timeout ao esperar resposta do servidor (10 segundos)\n");
    } else if (n == -1) {
        printf("Create: erro de conexão ou servidor fechou a conexão\n");
    } else {
        printf("Create: nenhuma resposta recebida\n");
    }
}

void cmd_close(struct sockaddr_in *server_addr, char *logged_uid, char *logged_pass, int *logged_in, char *args) {
    char EID[5];
    char command[128], response[128];
    int tcp_fd;
    
    if (!*logged_in) {
        printf("Não está logged in\n");
        return;
    }
    
    if (sscanf(args, "%4s", EID) != 1) {
        printf("Uso: close <EID>\n");
        return;
    }
    
    if (!validate_eid(EID)) {
        printf("Erro: EID inválido\n");
        return;
    }
    
    if ((tcp_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Erro ao criar socket TCP");
        return;
    }
    
    if (connect(tcp_fd, (struct sockaddr*)server_addr, sizeof(*server_addr)) < 0) {
        perror("Erro ao conectar");
        close(tcp_fd);
        return;
    }
    
    snprintf(command, sizeof(command), "CLS %s %s %s\n", logged_uid, logged_pass, EID);
    send_all_tcp(tcp_fd, command, strlen(command));
    
    int n = recv_line_tcp(tcp_fd, response, sizeof(response));
    close(tcp_fd);
    
    if (n > 0) {
        if (strncmp(response, "RCL OK", 6) == 0) {
            printf("Evento fechado com sucesso\n");
        } else if (strncmp(response, "RCL EID", 7) == 0) {
            printf("Close: evento não existe\n");
        } else if (strncmp(response, "RCL USR", 7) == 0) {
            printf("Close: não é o dono do evento\n");
        } else if (strncmp(response, "RCL FUL", 7) == 0) {
            printf("Close: evento esgotado\n");
        } else {
            printf("Close: %s\n", response);
        }
    }
}

void cmd_show(struct sockaddr_in *server_addr, char *args) {
    char EID[5], command[64];
    int tcp_fd;
    
    if (sscanf(args, "%4s", EID) != 1) {
        printf("Uso: show <EID>\n");
        return;
    }
    
    if (!validate_eid(EID)) {
        printf("Erro: EID inválido\n");
        return;
    }
    
    if ((tcp_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Erro ao criar socket TCP");
        return;
    }
    
    if (connect(tcp_fd, (struct sockaddr*)server_addr, sizeof(*server_addr)) < 0) {
        perror("Erro ao conectar");
        close(tcp_fd);
        return;
    }
    
    snprintf(command, sizeof(command), "SED %s\n", EID);
    send_all_tcp(tcp_fd, command, strlen(command));

    char tok[256];
    char uid[7] = "";
    char name[11] = "";
    char date[11] = "";
    char time_s[6] = "";
    char filename[256] = "";
    int capacity = 0;
    int reserved = 0;
    int state = -1;
    size_t filesize = 0;

    if (recv_token_tcp(tcp_fd, tok, sizeof(tok)) != 0 || strcmp(tok, "RSE") != 0) {
        printf("Show: erro ao receber resposta\n");
        close(tcp_fd);
        return;
    }
    if (recv_token_tcp(tcp_fd, tok, sizeof(tok)) != 0) {
        printf("Show: erro ao receber resposta\n");
        close(tcp_fd);
        return;
    }
    if (strcmp(tok, "NOK") == 0) {
        printf("Show: evento não encontrado\n");
        close(tcp_fd);
        return;
    }
    if (strcmp(tok, "ERR") == 0) {
        printf("Show: erro de formato\n");
        close(tcp_fd);
        return;
    }
    if (strcmp(tok, "OK") != 0) {
        printf("Show: %s\n", tok);
        close(tcp_fd);
        return;
    }

    if (recv_token_tcp(tcp_fd, uid, sizeof(uid)) != 0 ||
        recv_token_tcp(tcp_fd, name, sizeof(name)) != 0 ||
        recv_token_tcp(tcp_fd, date, sizeof(date)) != 0 ||
        recv_token_tcp(tcp_fd, time_s, sizeof(time_s)) != 0 ||
        recv_token_tcp(tcp_fd, tok, sizeof(tok)) != 0) {
        printf("Show: erro ao receber resposta\n");
        close(tcp_fd);
        return;
    }
    capacity = atoi(tok);

    if (recv_token_tcp(tcp_fd, tok, sizeof(tok)) != 0) {
        printf("Show: erro ao receber resposta\n");
        close(tcp_fd);
        return;
    }
    reserved = atoi(tok);

    if (recv_token_tcp(tcp_fd, filename, sizeof(filename)) != 0 ||
        recv_token_tcp(tcp_fd, tok, sizeof(tok)) != 0) {
        printf("Show: erro ao receber resposta\n");
        close(tcp_fd);
        return;
    }
    filesize = (size_t)strtoull(tok, NULL, 10);

    if (filesize > 10485760) {
        printf("Show: ficheiro demasiado grande\n");
        close(tcp_fd);
        return;
    }

    char peek2[2];
    ssize_t pn = recv(tcp_fd, peek2, sizeof(peek2), MSG_PEEK);
    if (pn == 2 && peek2[0] >= '0' && peek2[0] <= '3' && peek2[1] == ' ') {
        if (recv_token_tcp(tcp_fd, tok, sizeof(tok)) != 0) {
            printf("Show: erro ao receber resposta\n");
            close(tcp_fd);
            return;
        }
        state = atoi(tok);
    } else {
        if (is_date_past(date, time_s)) state = 0;
        else if (reserved >= capacity) state = 2;
        else state = 1;
    }

    printf("Dono: %s\n", uid);
    printf("Nome: %s\n", name);
    printf("Data: %s %s\n", date, time_s);
    printf("Capacidade: %d\n", capacity);
    printf("Reservados: %d\n", reserved);
    printf("Ficheiro: %s (%zu bytes)\n", filename, filesize);

    if (state == 3) printf("Estado: Fechado\n");
    else if (state == 2) printf("Estado: Esgotado\n");
    else if (state == 0) printf("Estado: Passado\n");
    else printf("Estado: Aberto\n");

    char *file_data = NULL;
    if (filesize > 0) {
        file_data = (char *)malloc(filesize);
        if (!file_data) {
            printf("Show: memória insuficiente\n");
            close(tcp_fd);
            return;
        }
        if (recv_all_tcp(tcp_fd, file_data, filesize) != 0) {
            printf("Show: erro ao receber ficheiro\n");
            free(file_data);
            close(tcp_fd);
            return;
        }
        char nl;
        (void)recv(tcp_fd, &nl, 1, MSG_DONTWAIT);

        char local_filename[300];
        snprintf(local_filename, sizeof(local_filename), "event_%s_%s", EID, filename);
        FILE *fp = fopen(local_filename, "wb");
        if (fp) {
            fwrite(file_data, 1, filesize, fp);
            fclose(fp);
            printf("Ficheiro guardado como: %s\n", local_filename);
            char cwd[1024];
            if (getcwd(cwd, sizeof(cwd)) != NULL) printf("Diretoria: %s\n", cwd);
        }
        free(file_data);
    }

    close(tcp_fd);
}

void cmd_reserve(struct sockaddr_in *server_addr, char *logged_uid, char *logged_pass, int *logged_in, char *args) {
    char EID[5];
    int seats;
    char command[128], response[128];
    int tcp_fd;
    
    if (!*logged_in) {
        printf("Não está logged in\n");
        return;
    }
    
    if (sscanf(args, "%4s %d", EID, &seats) != 2) {
        printf("Uso: reserve <EID> <seats>\n");
        return;
    }
    
    if (!validate_eid(EID)) {
        printf("Erro: EID inválido\n");
        return;
    }
    
    if (seats < 1 || seats > 999) {
        printf("Erro: Número inválido (1..999)\n");
        return;
    }
    
    if ((tcp_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Erro ao criar socket TCP");
        return;
    }
    
    if (connect(tcp_fd, (struct sockaddr*)server_addr, sizeof(*server_addr)) < 0) {
        perror("Erro ao conectar");
        close(tcp_fd);
        return;
    }
    
    snprintf(command, sizeof(command), "RID %s %s %s %d\n", logged_uid, logged_pass, EID, seats);
    send_all_tcp(tcp_fd, command, strlen(command));
    
    int n = recv_line_tcp(tcp_fd, response, sizeof(response));
    close(tcp_fd);
    
    if (n > 0) {
        if (strncmp(response, "RRI ACC", 7) == 0) {
            printf("Reserva aceite\n");
        } else if (strncmp(response, "RRI REJ", 7) == 0) {
            int remaining = -1;
            if (sscanf(response, "RRI REJ %d", &remaining) == 1 && remaining >= 0) {
                printf("Reserve: pedido rejeitado (restam %d lugares)\n", remaining);
            } else {
                printf("Reserve: pedido rejeitado\n");
            }
        } else if (strncmp(response, "RRI CLS", 7) == 0) {
            printf("Reserve: evento fechado\n");
        } else if (strncmp(response, "RRI SLD", 7) == 0) {
            printf("Reserve: evento esgotado\n");
        } else if (strncmp(response, "RRI PST", 7) == 0) {
            printf("Reserve: evento já ocorreu\n");
        } else if (strncmp(response, "RRI NLG", 7) == 0) {
            printf("Reserve: não autenticado\n");
        } else if (strncmp(response, "RRI WRP", 7) == 0) {
            printf("Reserve: password incorreta\n");
        } else if (strncmp(response, "RRI NOK", 7) == 0) {
            printf("Reserve: evento não existe/não ativo\n");
        } else if (strncmp(response, "RRI ERR", 7) == 0) {
            printf("Reserve: erro de formato\n");
        } else {
            printf("Reserve: %s\n", response);
        }
    }
}

void print_help() {
    printf("\nComandos disponíveis:\n");
    printf("  -login <UID> <password>              - Login (UID: 6 dig, pass: 8 alfanum)\n");
    printf("  -logout                              - Fazer logout\n");
    printf("  -unregister <password>               - Desregistar (pass: 8 alfanum)\n");
    printf("  -changepass <old_pass> <new_pass>    - Alterar pass (8 alfanum)\n");
    printf("  -create <name> <event_fname> <event_date> <num_attendees>\n");
    printf("      name: max 10 alfanum, event_date: DD-MM-YYYY HH:MM, num_attendees: 10..999\n");
    printf("  -close <EID>                         - Fechar evento (EID: 3 dig)\n");
    printf("  -list                                - Listar todos os eventos\n");
    printf("  -show <EID>                          - Mostrar detalhes (EID: 3 dig)\n");
    printf("  -reserve <EID> <seats>               - Reservar lugares\n");
    printf("  -myevents (mye)                      - Listar os meus eventos\n");
    printf("  -myreservations (myr)                - Listar as minhas reservas\n");
    printf("  -help                                - Mostrar esta ajuda\n");
    printf("  -exit                                - Sair do programa\n");
    printf("\n");
}

int main(int argc, char *argv[]) {
    int udp_fd;
    struct sockaddr_in server_addr;
    char logged_uid[7] = "";
    char logged_pass[20] = "";
    int logged_in = 0;
    char *server_ip = "127.0.0.1";
    char *port = DEFAULT_PORT;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            server_ip = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = argv[++i];
        }
    }
    
    if ((udp_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Erro ao criar socket UDP");
        return 1;
    }

    signal(SIGINT, handle_sigint);
    
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
    
    char input_line[BUFFER_SIZE];
    while (keep_running) {
        printf("> ");
        if (fgets(input_line, sizeof(input_line), stdin) == NULL) {
            if (!keep_running) break;
            if (errno == EINTR) continue;
            break;
        }
        
        input_line[strcspn(input_line, "\n")] = 0;
        
        char command[100];
        char args[BUFFER_SIZE];
        args[0] = '\0';
        
        int n = sscanf(input_line, "%99s %[^\n]", command, args);
        if (n < 1) continue;
        
        if (strcmp(command, "login") == 0) {
            cmd_login(udp_fd, &server_addr, logged_uid, logged_pass, &logged_in, args);
        } else if (strcmp(command, "logout") == 0) {
            cmd_logout(udp_fd, &server_addr, logged_uid, logged_pass, &logged_in, args);
        } else if (strcmp(command, "unregister") == 0) {
            cmd_unregister(udp_fd, &server_addr, logged_uid, logged_pass, &logged_in, args);
        } else if (strcmp(command, "changepass") == 0) {
            cmd_changepass(&server_addr, logged_uid, logged_pass, &logged_in, args);
        } else if (strcmp(command, "create") == 0) {
            cmd_create(&server_addr, logged_uid, logged_pass, &logged_in, args);
        } else if (strcmp(command, "close") == 0) {
            cmd_close(&server_addr, logged_uid, logged_pass, &logged_in, args);
        } else if (strcmp(command, "list") == 0) {
            cmd_list(&server_addr, logged_uid, &logged_in, args);
        } else if (strcmp(command, "show") == 0) {
            cmd_show(&server_addr, args);
        } else if (strcmp(command, "reserve") == 0) {
            cmd_reserve(&server_addr, logged_uid, logged_pass, &logged_in, args);
        } else if (strcmp(command, "myevents") == 0 || strcmp(command, "mye") == 0) {
            cmd_myevents(udp_fd, &server_addr, logged_uid, logged_pass, &logged_in, args);
        } else if (strcmp(command, "myreservations") == 0 || strcmp(command, "myr") == 0) {
            cmd_myreservations(udp_fd, &server_addr, logged_uid, logged_pass, &logged_in, args);
        } else if (strcmp(command, "help") == 0) {
            print_help();
        } else if (strcmp(command, "exit") == 0) {
            break;
        } else {
            printf("Comando desconhecido. Digite 'help' para ver comandos disponíveis.\n");
        }
    }
    
    close(udp_fd);
    printf("Cliente encerrado\n");
    
    return 0;
}