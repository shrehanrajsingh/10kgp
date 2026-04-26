# 10KGP

An internal peer-to-peer interaction framework for computers on a shared local network. The application runs entirely in the terminal, uses UDP broadcast for peer discovery, and is designed to be extended with additional communication modules over time.

---

## Overview

10KGP discovers other instances of itself running on the same Wi-Fi network, tracks their presence, and provides a simple command interface for interacting with them. Peer discovery is handled by the `emit` module, which periodically broadcasts a UDP pulse to `255.255.255.255` on port 9999. Any peer that receives the pulse records the sender and vice versa, building a live view of who is online without any central coordinator.

---

## Requirements

- C11-compatible compiler (GCC or Clang)
- CMake 3.10 or later
- POSIX-compatible OS (Linux or macOS)
- A shared local network with UDP broadcast enabled (most home and office routers support this by default)

---

## Building

```bash
git clone <repository-url>
cd 10kgp
mkdir build && cd build
cmake ..
make
```

The resulting binary is placed at `build/10kgp`.

To build with debug symbols:

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

---

## Running

```bash
./build/10kgp
```

On launch you will be prompted to enter a peer ID. This is a short identifier (up to 31 characters) that other peers on the network will see. It can be a username, hostname, or any unique string.

```
=== Peer Discovery ===

Enter your peer ID: alice
```

After entering your ID, the application sends an initial broadcast pulse and begins listening for other peers. The `>` prompt accepts commands.

---

## Commands

| Command | Description |
|---|---|
| `/online` | List all peers currently online |
| `/peers` | Alias for `/online` |
| `/count` | Print the number of online peers |
| `/refresh` | Send a broadcast pulse immediately |
| `/clear` | Clear the terminal screen |
| `/alias <command> <alias>` | Create a shorthand for a command |
| `/show-alias` | List all defined aliases |
| `/clear-alias <alias>` | Remove a specific alias |
| `/help` | Show the help message |
| `/exit` | Exit the application |
| `/quit` | Alias for `/exit` |

### Aliases

Aliases let you bind a shorter name to any built-in command for the duration of a session. They are not persisted between runs.

```
> /alias online o
Alias '/o' -> '/online' created.

> /o
Online peers (2):
...

> /show-alias
Aliases (1):
  Alias            -> Command
  ----------------    -------
  /o               -> /online

> /clear-alias o
Alias '/o' cleared.
```

---

## Architecture

```
10kgp/
  main.c        -- CLI, command dispatch, peer state management
  main.h        -- Top-level includes
  header.h      -- Common system and POSIX headers
  emit/
    emit.c      -- UDP broadcast implementation
    emit.h      -- Public API for the emit module
  CMakeLists.txt
```

### Peer Discovery (emit)

The `emit` module handles all network I/O. It opens a single UDP socket with `SO_BROADCAST` and `SO_REUSEADDR` enabled, bound to port 9999. Every 5 seconds the main loop calls `emit_pulse()`, which sends the local peer ID as a raw ASCII packet to the broadcast address. Incoming packets are received in the same loop via `select()` and delivered to a caller-supplied callback.

Peers are tracked in a fixed-size array of up to 64 entries. Any peer not seen within 30 seconds is considered stale and removed from the list.

Constants:

| Constant | Value | Purpose |
|---|---|---|
| `EMIT_MAX_PEERS` | 64 | Maximum number of tracked peers |
| `EMIT_PEER_ID_LEN` | 32 | Maximum peer ID string length (including null terminator) |
| `EMIT_PACKET_SIZE` | 256 | Maximum UDP packet size |
| `PEER_TIMEOUT_SEC` | 30 | Seconds before a peer is considered offline |
| `PULSE_INTERVAL_SEC` | 5 | Seconds between automatic broadcast pulses |

### Main Loop

The main loop uses `select()` to monitor both standard input and the UDP socket simultaneously, with a 1-second timeout. This allows the application to process network events and user input without blocking either path. The prompt is only reprinted when user input has been consumed, so incoming peer activity does not corrupt partial input lines.

---

## Planned Modules

| Module | Description | Status |
|---|---|---|
| `emit` | UDP broadcast peer discovery | In progress |
| `lms` | Local Mail Server | Not started |
| `cft` | Chunk-based File Transfer | Not started |

---

## Contributing

Pull requests are welcome. When adding a new module, place it in its own subdirectory following the pattern of `emit/`, register its sources in `CMakeLists.txt`, and document its public API in a `README.md` within the module directory.

---

Made by [shrehanrajsingh](https://github.com/shrehanrajsingh)
