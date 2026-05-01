#ifndef EMIT_H

#include "../header.h"

#define EMIT_MAX_PEERS 64
#define EMIT_PACKET_SIZE 256
#define EMIT_PEER_ID_LEN 32

/* Administratively scoped IPv4 multicast (RFC 2365); complements UDP broadcast. */
#define EMIT_MULTICAST_GROUP "239.255.43.210"

/* Peer information struct passed to callbacks */
typedef struct
{
  char peer_id[EMIT_PEER_ID_LEN];
  struct in_addr ip_address;
  uint16_t port;
  time_t timestamp;
} peer_info_t;

/* Callback function type - called when a new peer is detected */
typedef void (*peer_callback_t) (const peer_info_t *peer, void *user_data);

/* Initialize the emitter on the given port */
int emit_init (uint16_t port);

/* Set this peer's unique identifier */
void emit_set_peer_id (const char *id);

/* Send discovery pulses (broadcast + multicast + LAN sweep) */
int emit_pulse (void);

/* Listen for peer signals. Returns when timeout_ms elapses or on error */
int emit_listen (peer_callback_t callback, void *user_data, int timeout_ms);

/* Drain queued UDP datagrams without blocking (MSG_DONTWAIT). Call each main-
 * loop iteration after select(): some stacks omit the socket from the readable
 * set while datagrams are already queued; unconditional drain avoids that. */
int emit_recv_drain (peer_callback_t callback, void *user_data);

/* Cleanup and close sockets */
void emit_cleanup (void);

/* Get the socket file descriptor for select() */
int emit_get_socket_fd (void);

#endif
