#ifndef EMIT_BCAST_H
#define EMIT_BCAST_H

#include <stddef.h>

struct sockaddr_in;

#define EMIT_BCAST_MAX_TARGETS 32

/* Gather IPv4 broadcast addresses (subnet-directed per interface + limited 255.255.255.255).
   Fills out[0..n-1] with sin_family=AF_INET and sin_addr set; sin_port must be set by caller.
   Returns n (always >= 1 if max_out >= 1). */
size_t emit_bcast_collect (struct sockaddr_in *out, size_t max_out);

#endif
