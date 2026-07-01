#include "protocol.h"
#include <netinet/in.h>
#include <string.h>

int parse_header(db_header_t *header) {

    if (header->magic != MAGIC_BYTE) {
        return -1; // Invalid packet protocol identification
    }

    // Convert network byte order to host byte order
    header->key_len = ntohs(header->key_len);
    header->val_len = ntohl(header->val_len);

    return 0;
}
