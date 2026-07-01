#ifndef CACHE_H
#define CACHE_H

#include <stdint.h>

typedef struct cache_node {
  char *key;
  uint8_t *value;
  uint32_t val_len;
  struct cache_node *next;
} cache_node_t;

typedef struct {
  cache_node_t **buckets;
  uint32_t size;
} cache_t;

cache_t* cache_create(uint32_t size);
int cache_set(cache_t *cache, const char *key, const uint8_t *value, uint32_t val_len);
cache_node_t* cache_get(cache_t *cache, const char *key);
void cache_free(cache_t *cache);

#endif
