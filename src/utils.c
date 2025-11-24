#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include "utils.h"

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 1024
#endif

int validate_uid(const char *uid) {
    if (strlen(uid) != 6) return 0;
    for (int i = 0; i < 6; i++)
        if (!isdigit((unsigned char)uid[i])) return 0;
    return 1;
}

int validate_password(const char *password) {
    if (strlen(password) != 8) return 0;
    for (int i = 0; i < 8; i++)
        if (!isalnum((unsigned char)password[i])) return 0;
    return 1;
}

int validate_event_name(const char *name) {
    size_t len = strlen(name);
    if (len == 0 || len > 10) return 0;
    for (size_t i = 0; i < len; i++)
        if (!isalnum((unsigned char)name[i])) return 0;
    return 1;
}

int validate_date(const char *date) {
    // Format: dd-mm-yyyy
    if (strlen(date) != 10) return 0;
    if (date[2] != '-' || date[5] != '-') return 0;
    for (int i = 0; i < 10; i++) {
        if (i == 2 || i == 5) continue;
        if (!isdigit((unsigned char)date[i])) return 0;
    }
    return 1;
}

int validate_time(const char *time) {
    // Format: hh:mm
    if (strlen(time) != 5) return 0;
    if (time[2] != ':') return 0;
    for (int i = 0; i < 5; i++) {
        if (i == 2) continue;
        if (!isdigit((unsigned char)time[i])) return 0;
    }
    return 1;
}

int validate_eid(const char *eid) {
    if (strlen(eid) != 3) return 0;
    for (int i = 0; i < 3; i++)
        if (!isdigit((unsigned char)eid[i])) return 0;
    return 1;
}

// TCP I/O robustas - loops até byte count completo
int send_all_tcp(int fd, const void *buf, size_t len) {
    const char *ptr = (const char *)buf;
    while (len > 0) {
        ssize_t n = write(fd, ptr, len);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            return -1;
        }
        ptr += n;
        len -= n;
    }
    return 0;
}

int recv_all_tcp(int fd, void *buf, size_t len) {
    char *ptr = (char *)buf;
    while (len > 0) {
        ssize_t n = read(fd, ptr, len);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            return -1;
        }
        ptr += n;
        len -= n;
    }
    return 0;
}

int recv_line_tcp(int fd, char *buf, size_t maxlen) {
    size_t i = 0;
    while (i < maxlen - 1) {
        char c;
        ssize_t n = read(fd, &c, 1);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            return -1;
        }
        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = '\0';
    return (int)i;
}

static int has_token(const char *reply, const char *token) {
    // tokens are separated by spaces/newlines; ensure full match
    const char *p = reply;
    size_t len = strlen(token);
    while ((p = strstr(p, token))) {
        int start_ok = (p == reply) || isspace((unsigned char)*(p - 1));
        int end_ok = isspace((unsigned char)p[len]) || p[len] == '\0';
        if (start_ok && end_ok) return 1;
        p += len; // continue searching
    }
    return 0;
}

void show_reply(const char *reply) {
    if (strncmp(reply, "RLI ", 4) == 0) {
        if (has_token(reply, "OK")) puts("Login: sucesso");
        else if (has_token(reply, "REG")) puts("Login: novo utilizador registado");
        else if (has_token(reply, "NOK")) puts("Login: password incorreta");
        else if (has_token(reply, "ERR")) puts("Login: erro de formato");
        else printf("Login: resposta desconhecida (%s)\n", reply);
    } else if (strncmp(reply, "RLO ", 4) == 0) {
        if (has_token(reply, "OK")) puts("Logout: sucesso");
        else if (has_token(reply, "WRP")) puts("Logout: password incorreta");
        else if (has_token(reply, "UNR")) puts("Logout: utilizador não registado");
        else if (has_token(reply, "NOK")) puts("Logout: não estava autenticado");
        else if (has_token(reply, "ERR")) puts("Logout: erro de formato");
        else printf("Logout: resposta desconhecida (%s)\n", reply);
    } else if (strncmp(reply, "RUR ", 4) == 0) {
        if (has_token(reply, "OK")) puts("Unregister: utilizador removido");
        else if (has_token(reply, "WRP")) puts("Unregister: password incorreta");
        else if (has_token(reply, "UNR")) puts("Unregister: utilizador não existe");
        else if (has_token(reply, "NOK")) puts("Unregister: não estava autenticado");
        else if (has_token(reply, "ERR")) puts("Unregister: erro de formato");
        else printf("Unregister: resposta desconhecida (%s)\n", reply);
    } else if (strncmp(reply, "RME ", 4) == 0) {
        if (has_token(reply, "OK")) {
            const char *p = strstr(reply, "OK");
            p += 2;
            while (*p == ' ') p++;
            if (*p == '\n' || *p == '\0') {
                puts("Meus eventos: (nenhum listado)");
            } else {
                puts("Meus eventos:");
                char temp[BUFFER_SIZE];
                strncpy(temp, p, sizeof(temp) - 1);
                temp[sizeof(temp) - 1] = '\0';
                char *tok = strtok(temp, " \n");
                while (tok) {
                    char *eid = tok;
                    tok = strtok(NULL, " \n");
                    if (!tok) break;
                    char *state = tok;
                    printf("  Evento %s estado %s\n", eid, state);
                    tok = strtok(NULL, " \n");
                }
            }
        } else if (has_token(reply, "NLG")) puts("Myevents: não autenticado");
        else if (has_token(reply, "WRP")) puts("Myevents: password incorreta");
        else if (has_token(reply, "NOK")) puts("Myevents: sem eventos");
        else if (has_token(reply, "ERR")) puts("Myevents: erro de formato");
        else printf("Myevents: resposta desconhecida (%s)\n", reply);
    } else if (strncmp(reply, "RMR ", 4) == 0) {
        if (has_token(reply, "OK")) {
            const char *p = strstr(reply, "OK");
            p += 2;
            while (*p == ' ') p++;
            if (*p == '\n' || *p == '\0') {
                puts("Minhas reservas: (nenhuma)");
            } else {
                puts("Minhas reservas:");
                char temp[BUFFER_SIZE];
                strncpy(temp, p, sizeof(temp) - 1);
                temp[sizeof(temp) - 1] = '\0';
                char *tok = strtok(temp, " \n");
                while (tok) {
                    char *eid = tok;
                    tok = strtok(NULL, " \n"); if (!tok) break;
                    char *date = tok;
                    tok = strtok(NULL, " \n"); if (!tok) break;
                    char *time = tok;
                    tok = strtok(NULL, " \n"); if (!tok) break;
                    char *value = tok;
                    printf("  %s em %s %s lugares=%s\n", eid, date, time, value);
                    tok = strtok(NULL, " \n");
                }
            }
        } else if (has_token(reply, "NLG")) puts("Myreservations: não autenticado");
        else if (has_token(reply, "WRP")) puts("Myreservations: password incorreta");
        else if (has_token(reply, "NOK")) puts("Myreservations: sem reservas");
        else if (has_token(reply, "ERR")) puts("Myreservations: erro de formato");
        else printf("Myreservations: resposta desconhecida (%s)\n", reply);
    } else {
        printf("Resposta desconhecida: %s\n", reply);
    }
}