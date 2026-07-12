#ifndef BTREE_H
#define BTREE_H

#include <stdint.h>
#include "pager.h"
#include <stdbool.h>

#define MAX_KEYS_LEAF 140
#define MAX_KEYS_INTERNAL 250

typedef enum {
  NODE_INTERNAL = 0x01,
  NODE_LEAF = 0x02
} node_type_t;

typedef struct __attribute__((packed)) {
  uint8_t type;
  uint16_t num_keys;
  uint64_t next_sibling;
  uint64_t keys[MAX_KEYS_LEAF];
  uint32_t val_lens[MAX_KEYS_LEAF];
  uint8_t values[MAX_KEYS_LEAF][16];
} btree_leaf_t;

typedef struct __attribute__((packed)){
  uint8_t type;
  uint16_t num_keys;
  uint64_t keys[MAX_KEYS_INTERNAL];
  uint64_t child_pointers[MAX_KEYS_INTERNAL + 1];
} btree_internal_t;

typedef struct {
  pager_t *pager;
  uint64_t root_page_id;
} btree_t;

btree_t* btree_open(pager_t *pager, uint64_t root_page_id);
int btree_insert(btree_t *tree, uint64_t key, const uint8_t *value, uint32_t val_len);
bool btree_get(btree_t *tree, uint64_t key, uint8_t *val_out, uint32_t *len_out);

#endif
