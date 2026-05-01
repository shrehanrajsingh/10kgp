#ifndef EMIT_LAN_SCAN_H
#define EMIT_LAN_SCAN_H

#include "emit_sock.h"
#include <stddef.h>
#include <stdint.h>

/* Unicast same payload to every usable host on local /24-sized subnets (best-effort).
   Skips broader subnets to avoid huge scans. Returns count of successful sendto calls. */
size_t emit_lan_sweep_send (emit_sock_t sock, uint16_t port_host_order,
                            const char *pkt, size_t pkt_len);

#endif
