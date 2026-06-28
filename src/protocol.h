#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#define MAGIC_BYTE 0x32

typedef enum {
    OP_GET = 0x01,
    OP_SET = 0x02,
    OP_DEL = 0x03
} opcode_t;

// Packed structure ensures zero padding compiler manipulation
typedef struct __attribute__((packed)) {
    uint8_t magic;
    uint8_t opcode;
    uint16_t key_len;
    uint32_t val_len;
} db_header_t;

typedef struct {
    db_header_t header;
    char *key;
    uint8_t *value;
} db_packet_t;

// Parses a buffer into a structured packet. Returns 0 on success.
int parse_header(const uint8_t *buffer, db_header_t *header);

#endif
