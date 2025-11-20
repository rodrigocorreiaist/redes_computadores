#ifndef PROTOCOL_TCP_H
#define PROTOCOL_TCP_H

#include <sys/socket.h>
#include <netinet/in.h>
#include "event_server.h"

void process_tcp_command(int client_fd, int verbose_mode);

#endif // PROTOCOL_TCP_H
