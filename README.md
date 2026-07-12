# Networked Storage Engine

A Linux-focused key-value storage engine written from scratch in C. The project combines a nonblocking TCP server, a compact binary wire protocol, an in-memory hash cache, and a page-managed on-disk B-tree prototype.

This repository is intended to demonstrate low-level systems engineering: socket programming, event-driven I/O, binary protocol design, fixed-size page storage, explicit disk synchronization, and manual memory management.

## What It Does

- Accepts TCP client connections on port `8080`.
- Uses `epoll` with nonblocking sockets to multiplex client I/O.
- Parses a custom binary protocol with network-byte-order fields.
- Supports `SET` and `GET` operations through the test client.
- Stores hot values in an in-memory hash table cache.
- Persists values to `engine.db` through a fixed-size 4 KB pager.
- Indexes persisted values with a B-tree-inspired page layout.
- Recovers previously written data from disk on restart for supported values.

## Architecture

```text
TCP clients
    |
    v
Nonblocking socket server
    |
    v
epoll event loop
    |
    v
Binary protocol parser
    |
    +--> In-memory hash cache
    |
    +--> B-tree index
              |
              v
          4 KB pager
              |
              v
          engine.db
```

### Core Components

| Component | Files | Responsibility |
| --- | --- | --- |
| Network layer | `src/network.c`, `src/network.h` | Creates the server socket, configures nonblocking mode, registers file descriptors with `epoll`, and reads from client sockets. |
| Wire protocol | `src/protocol.c`, `src/protocol.h` | Defines and validates the binary request header, including endian conversion for multi-byte fields. |
| Server orchestration | `src/main.c` | Runs the event loop, accepts clients, parses requests, coordinates cache and disk access, and sends responses. |
| Cache | `src/cache.c`, `src/cache.h` | Provides an in-memory chained hash table for fast key lookups. |
| Pager | `src/pager.c`, `src/pager.h` | Reads and writes fixed-size 4 KB pages with `pread`, `pwrite`, and `fsync`. |
| B-tree storage | `src/btree.c`, `src/btree.h` | Maintains a page-backed sorted leaf layout with basic split behavior and lookup support. |
| Test client | `test_client.py` | Sends binary `SET` and `GET` requests to the server for local smoke testing. |

## Binary Wire Protocol

Each request starts with an 8-byte packed header followed by the key bytes and optional value bytes.

| Field | Size | Description |
| --- | ---: | --- |
| Magic byte | 1 byte | Protocol identifier. Must be `0x32`. |
| Opcode | 1 byte | `0x01` = `GET`, `0x02` = `SET`, `0x03` = `DEL` reserved. |
| Key length | 2 bytes | Unsigned 16-bit length encoded in network byte order. |
| Value length | 4 bytes | Unsigned 32-bit length encoded in network byte order. |
| Payload | Variable | Key bytes followed by value bytes. `GET` requests send only a key. |

Example Python packing format:

```python
struct.pack("!BBHI", 0x32, opcode, key_len, value_len)
```

## Build Requirements

This project currently depends on Linux `epoll`, so build and run it on Linux or inside a Linux VM/container.

Required tools:

- `gcc`
- `make`
- `python3` for the test client

## Build

```bash
make
```

This produces the `engine_server` executable.

To remove build artifacts:

```bash
make clean
```

## Run

Start the server:

```bash
./engine_server
```

In another terminal, send a `SET` followed by a `GET`:

```bash
python3 test_client.py
```

Expected client behavior:

- The `SET` request receives `STORED`.
- The `GET` request receives the stored value.

## Implementation Notes

- Keys are hashed to 64-bit integers before being written to the B-tree layer.
- The in-memory cache stores full key strings and full value buffers.
- The current B-tree page format stores up to 16 bytes per persisted value.
- The pager writes fixed 4 KB pages and calls `fsync` after disk mutations.
- `engine.db` is the local backing file used by the storage engine.

## Current Limitations

This is a systems prototype, not a production database. The most important limitations are:

- `DEL` is defined in the protocol but not implemented in the server.
- Persistent values are capped at 16 bytes in the B-tree page format.
- Disk lookups use hashed keys, so hash collisions are not resolved by comparing original keys on disk.
- Internal-node traversal is incomplete after B-tree splits; routing currently targets the first child in several paths.
- The nonblocking read path does not yet maintain per-connection input buffers, so fragmented TCP packets can close or truncate a request.
- There is no authentication, encryption, replication, crash-recovery log, or concurrency control.
- The build is Linux-specific because it uses `epoll`.

## Roadmap

High-impact next steps:

1. Add per-connection request buffers so the protocol parser handles partial reads correctly.
2. Store full values using overflow pages or variable-length record pages.
3. Persist original keys alongside hashed keys to resolve collisions.
4. Complete B-tree internal-node insertion, child routing, and multi-level splits.
5. Implement `DEL` with tombstones or page compaction.
6. Add automated C unit tests and integration tests that start the server, send binary packets, restart, and verify persistence.
7. Add a Linux CI workflow for build and smoke-test coverage.

## Repository Layout

```text
.
├── Makefile
├── README.md
├── engine.db
├── test_client.py
└── src
    ├── btree.c
    ├── btree.h
    ├── cache.c
    ├── cache.h
    ├── main.c
    ├── network.c
    ├── network.h
    ├── pager.c
    ├── pager.h
    ├── protocol.c
    └── protocol.h
```
