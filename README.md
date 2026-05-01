# 10KGP

An internal peer-to-peer interaction framework for computers on a shared local network. The application runs entirely in the terminal, uses UDP broadcast for peer discovery, and is designed to be extended with additional communication modules over time.

---

## Overview

10KGP discovers other instances of itself running on the same Wi-Fi network, tracks their presence, and provides a simple command interface for interacting with them. Peer discovery is handled by the `emit` module, which periodically broadcasts a UDP pulse to `255.255.255.255` on port 9999. Any peer that receives the pulse records the sender and vice versa, building a live view of who is online without any central coordinator.

---

## Requirements

- C11-compatible compiler (GCC, Clang, or MSVC)
- CMake 3.10 or later
- On Linux or macOS: a POSIX-like environment
- On Windows: Visual Studio 2019+ or MinGW-w64 with Winsock2; CMake links `ws2_32` for `10kgp`
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

The resulting binary is placed at `build/10kgp` (or `build/10kgp.exe` on Windows). The `persi_demo` tool is built as `build/persi_demo` (or `persi_demo.exe`).

### Windows

From a **Developer Command Prompt** or any shell where `cmake` and your compiler are on `PATH`:

```bat
cd 10kgp
mkdir build && cd build
cmake -G "Ninja" ..
cmake --build .
```

Use a Visual Studio generator if you prefer (`cmake -G "Visual Studio 17 2022" -A x64 ..`). Run `10kgp.exe` from the build directory. Networking uses Winsock2 via [emit/win_emit.c](emit/win_emit.c); console input uses [win_stdin.c](win_stdin.c) so stdin is not mixed with Winsock `select`.

To build with debug symbols (Unix-style generators):

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

On Windows with Ninja or MSBuild, add `-DCMAKE_BUILD_TYPE=Debug` when configuring if your generator respects it.

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
  header.h      -- Common headers (POSIX); on Windows includes win_header.h
  win_header.h  -- Winsock2 and Windows C runtime includes
  win_stdin.c   -- Windows-only background stdin reader (used with Winsock select)
  win_compat.h  -- Small portability macros (e.g. unlink)
  emit/
    emit.c          -- UDP broadcast (POSIX)
    emit_bcast.c    -- Collect subnet + limited broadcast targets
    emit_mcast.c    -- IPv4 multicast join + multicast destination
    emit_lan_scan.c -- Unicast sweep on local /24-sized subnets
    win_emit.c      -- UDP broadcast (Windows Winsock2)
    emit.h          -- Public API for the emit module
  persi/        -- File-backed store (portable C)
  CMakeLists.txt
```

### Peer Discovery (emit)

The `emit` module handles all network I/O. It opens a single UDP socket with `SO_BROADCAST` and `SO_REUSEADDR` enabled, bound to port 9999. Every 5 seconds `emit_pulse()` sends the peer ID on **three paths**: (1) IPv4 broadcast addresses from [emit/emit_bcast.c](emit/emit_bcast.c), (2) UDP multicast to `239.255.43.210` (same port), after joining the group via [emit/emit_mcast.c](emit/emit_mcast.c), and (3) **directed unicast** to every usable host on each local IPv4 subnet whose mask is “LAN-sized” (at most 254 hosts—typically `/24`, e.g. `192.168.1.1`–`192.168.1.254`) via [emit/emit_lan_scan.c](emit/emit_lan_scan.c). The unicast sweep is intentionally aggressive so discovery still works when Wi‑Fi APs block broadcast and multicast between wireless clients.

Set environment variable `10KGDP_TRACE=1` to print a short line each pulse (stderr).

When data is ready on the socket, `emit_listen()` drains **all** queued UDP datagrams (`MSG_DONTWAIT`) so bursts are not missed after `select()`.

If peers still never appear: confirm both machines are on the same LAN subnet, turn off **AP / client isolation** on the router if enabled, allow **UDP port 9999** inbound on host firewalls, and avoid VPN interfaces that capture the default route away from Wi‑Fi.

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
