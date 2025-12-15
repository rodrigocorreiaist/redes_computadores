#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

int validate_uid(const char *uid);
int validate_password(const char *password);
int validate_event_name(const char *name);
int validate_date(const char *date);
int validate_time(const char *time);
int validate_eid(const char *eid);
int is_date_past(const char *date, const char *time);
void get_current_date(char *buffer);
void show_reply(const char *reply);

int send_all_tcp(int fd, const void *buf, size_t len);
int recv_all_tcp(int fd, void *buf, size_t len);
int recv_line_tcp(int fd, char *buf, size_t maxlen);

#endif