#include "cache.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

uint32_t hash_function (const char *str, uint32_t max_buckets) {
  uint32_t hash = 5381;
  int c;
  while ((c = *str++)){
    hash = ((hash<<5)+hash)+c;
  }
  return hash % max_buckets;
}

cache_t* cache_create(uint32_t size){
  cache_t *cache = malloc(sizeof(cache_t));
  cache->size = size;
  cache->buckets = calloc(size, sizeof(cache_node_t*));
  return cache;
}

int cache_set(cache_t *cache, const char *key, const uint8_t *value, uint32_t val_len) {
  uint32_t bucket = hash_function(key, cache->size);
  cache_node_t *curr = cache->buckets[bucket];

  while (curr != NULL){
    if(strcmp(curr->key, key) == 0){
      free(curr->value);
      curr->value = malloc(val_len);
      memcpy(curr->value, value, val_len);
      curr->val_len = val_len;
      return 0;
    }
    curr = curr->next;
  }

  cache_node_t *new_node = malloc(sizeof(cache_node_t));
  new_node->key = strdup(key);
  new_node->value = malloc(val_len);
  memcpy(new_node->value, value, val_len);
  new_node->val_len = val_len;

  new_node->next = cache->buckets[bucket];
  cache->buckets[bucket] = new_node;
  return 1;
}

cache_node_t* cache_get(cache_t *cache, const char *key){
  uint32_t bucket = hash_function(key, cache->size);
  cache_node_t *curr = cache->buckets[bucket];

  while(curr != NULL){
    if (strcmp(curr->key, key) == 0){
      return curr;
    }
    curr = curr->next;
  }
  return NULL; 
}

void cache_free(cache_t *cache){
  for (uint32_t i = 0;i<cache->size;i++){
    cache_node_t *curr = cache->buckets[i];
    while (curr != NULL){
      cache_node_t *tmp = curr;
      curr = curr->next;
      free(tmp->key);
      free(tmp->value);
      free(tmp);
    }
  }
  free(cache->buckets);
  free(cache);
}
