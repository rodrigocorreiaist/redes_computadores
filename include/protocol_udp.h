#ifndef PROTOCOL_UDP_H
#define PROTOCOL_UDP_H

#include <sys/socket.h>
#include <netinet/in.h>
#include "event_server.h"

void process_udp_command(int udp_fd, char *buffer, ssize_t n,
                         struct sockaddr_in *client_addr, socklen_t addr_len,
                         int verbose_mode);

#endif
