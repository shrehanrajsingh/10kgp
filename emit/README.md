# Emit - Peer Discovery Framework

A lightweight UDP broadcast-based peer discovery framework for detecting other computers on the local network.

## Overview

The `emit` module provides a simple API for:
- Broadcasting your presence to other peers on the network via UDP broadcast to `255.255.255.255`
- Listening for broadcast pulses from other peers
- Receiving callbacks when new peers are detected

## API Reference

### Types

```c
/* Peer information passed to your callback */
typedef struct {
    char peer_id[EMIT_PEER_ID_LEN];   /* Peer's unique identifier */
    struct in_addr ip_address;         /* Peer's IP address */
    uint16_t port;                     /* Peer's listening port */
    time_t timestamp;                  /* Discovery timestamp */
} peer_info_t;

/* Callback function type */
typedef void (*peer_callback_t)(const peer_info_t *peer, void *user_data);
```

### Functions

#### `emit_init(uint16_t port)`

Initialize the emitter and bind to the specified port.

- **Returns:** `0` on success, `-1` on error
- **Side effects:** Creates a UDP socket with `SO_BROADCAST` and `SO_REUSEADDR` options

#### `emit_set_peer_id(const char *id)`

Set the unique identifier for this peer. This ID is sent in broadcast pulses.

- **Parameters:** `id` - null-terminated string (max 31 characters)

#### `emit_pulse(void)`

Send a broadcast pulse to `255.255.255.255` on the configured port.

- **Returns:** `0` on success, `-1` on error

#### `emit_listen(peer_callback_t callback, void *user_data, int timeout_ms)`

Listen for incoming peer pulses. Blocks until a packet is received or timeout elapses.

- **Parameters:**
  - `callback` - function called when a peer is detected
  - `user_data` - opaque pointer passed to your callback
  - `timeout_ms` - maximum time to wait (milliseconds)
- **Returns:** `0` on success or timeout, `-1` on error

#### `emit_cleanup(void)`

Close sockets and release resources.

## Usage Example

```c
#include "emit/emit.h"
#include <stdio.h>

void on_peer_detected(const peer_info_t *peer, void *user_data) {
    printf("New peer detected: %s at %s:%d\n",
           peer->peer_id,
           inet_ntoa(peer->ip_address),
           peer->port);
}

int main(void) {
    /* Initialize on port 9999 */
    if (emit_init(9999) < 0) {
        fprintf(stderr, "Failed to initialize\n");
        return 1;
    }

    /* Set our peer ID */
    emit_set_peer_id("my-peer-001");

    /* Send a broadcast pulse */
    emit_pulse();

    /* Listen for peers for 5 seconds */
    printf("Listening for peers...\n");
    emit_listen(on_peer_detected, NULL, 5000);

    /* Cleanup */
    emit_cleanup();
    return 0;
}
```

## Packet Format

The broadcast packet contains the sender's peer ID as a null-terminated ASCII string.

```
+-----------------------------------+
| peer_id (null-terminated string)  |
+-----------------------------------+
```

## Building

```bash
mkdir build && cd build
cmake ..
make
```

## Platform Notes

- **Linux/macOS:** Fully supported with POSIX sockets
- **Windows:** Requires Winsock2 adaptation (`winsock2.h` instead of POSIX headers)
- **macOS:** May require network entitlements for broadcast in sandboxed apps

## Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `EMIT_MAX_PEERS` | 64 | Maximum tracked peers |
| `EMIT_PACKET_SIZE` | 256 | Maximum packet size |
| `EMIT_PEER_ID_LEN` | 32 | Maximum peer ID length |
