# Networked Storage Engine

An asynchronous, wire-protocol storage engine built from scratch in C. The architecture leverages low-level Linux primitives (`epoll`) for high-performance network multiplexing, combined with a custom binary wire protocol and an intended page-managed B-Tree storage backend.

## Architecture Blueprint

The engine uses a thread-per-core event loop model designed to separate non-blocking network I/O from blocking disk operations:

[Client 1]

[Client 2] --> [ epoll Event Loop ] --> [ Worker Thread Pool ] --> [ Page Manager ] --> [ Disk ]
[Client 3] /                                                           |
(In-Memory B-Tree)

1. **Multiplexer (`epoll`):** A single-threaded event loop handles high-concurrency client connections using Edge-Triggered (`EPOLLET`) non-blocking sockets.
2. **Wire Protocol Parser:** Parses incoming network streams into structured database operations based on a custom packed binary specification.
3. **Execution Pool:** Network loops offload blocking disk operations to a worker thread pool to prevent blocking the network multiplexer.
4. **Storage Engine:** Utilizes an on-disk B-Tree layout operating on fixed 4KB pages managed by a low-level block manager.

## Binary Wire Protocol

The custom protocol operates on a strict header-body framework optimized for fast parsing and zero-padding manipulation:

| Field | Size | Description |
| :--- | :--- | :--- |
| **Magic Byte** | 1 byte | Protocol verification token (Fixed to `0x32`). |
| **Opcode** | 1 byte | Command action (`0x01`=GET, `0x02`=SET, `0x03`=DEL). |
| **Key Length** | 2 bytes (uint16) | Size of the key string (Parsed via `ntohs`). |
| **Value Length**| 4 bytes (uint32) | Size of the value data payload (Parsed via `ntohl`). |
| **Payload** | Variable | Raw sequential Key bytes followed by Value bytes. |

## Project Structure

```text
.
├── Makefile          # Build configuration
├── README.md         # Documentation
├── .gitignore        # Git ignore rules
└── src
    ├── main.c        # Event loop coordinator & orchestrator
    ├── network.c     # Low-level socket options & non-blocking I/O
    ├── network.h     # Network primitives header
    ├── protocol.c    # Byte stream serialization & protocol validation
    └── protocol.h    # Packed structures & opcode schemas
