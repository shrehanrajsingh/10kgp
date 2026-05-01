#ifndef EMIT_MCAST_H
#define EMIT_MCAST_H

#include "emit_sock.h"
#include <stddef.h>
#include <stdint.h>

struct sockaddr_in;

/* Join multicast group on each IPv4 interface (best-effort). Returns 0 if at least one join succeeded. */
int emit_mcast_join (emit_sock_t sock);

void emit_mcast_leave (emit_sock_t sock);

void emit_mcast_dest (struct sockaddr_in *dst, uint16_t port_host_order);

#endif
