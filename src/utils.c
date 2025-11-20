#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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