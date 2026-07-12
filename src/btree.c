#include "btree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

btree_t* btree_open(pager_t *pager, uint64_t root_page_id){
  btree_t *tree = malloc(sizeof(btree_t));
  tree->pager = pager;
  tree->root_page_id = root_page_id;

  if (pager->total_pages <= root_page_id){
    uint8_t buffer[PAGE_SIZE];
    memset(buffer, 0, PAGE_SIZE);

    btree_leaf_t *root = (btree_leaf_t*)buffer;
    root->type = NODE_LEAF;
    root->num_keys = 0;
    root->next_sibling = 0;

    pager_write_page(pager, root_page_id, buffer);
    pager_sync(pager);
  }
  return tree;
}

static int leaf_find_key_slot(btree_leaf_t *leaf, uint64_t key){
  int low=0, high=leaf->num_keys - 1;
  while (low<=high){
    int mid = low + (high - low) / 2;
    if (leaf->keys[mid] == key) return mid;
    if (leaf->keys[mid] < key) low = mid + 1;
    else high = mid - 1;
  }
  return low;
}

static void leaf_split_and_insert(btree_t *tree, uint64_t root_id, btree_leaf_t *left_leaf, uint64_t key, const uint8_t *value, uint32_t val_len){
  uint8_t right_buffer[PAGE_SIZE];
  memset(right_buffer, 0, PAGE_SIZE);
  btree_leaf_t *right_leaf = (btree_leaf_t*)right_buffer;
  right_leaf->type = NODE_LEAF;

  int split_idx = left_leaf->num_keys / 2;
  int move_count = left_leaf->num_keys - split_idx;

  memcpy(right_leaf->keys, &left_leaf->keys[split_idx], move_count * sizeof(uint64_t));
  memcpy(right_leaf->val_lens, &left_leaf->val_lens[split_idx], move_count * sizeof(uint32_t));
  memcpy(right_leaf->values, &left_leaf->values[split_idx], move_count * 16);

  left_leaf->num_keys = split_idx;
  right_leaf->num_keys = move_count;

  if (key >= right_leaf->keys[0]) {
        int idx = leaf_find_key_slot(right_leaf, key);
        memmove(&right_leaf->keys[idx + 1], &right_leaf->keys[idx], (right_leaf->num_keys - idx) * sizeof(uint64_t));
        memmove(&right_leaf->val_lens[idx + 1], &right_leaf->val_lens[idx], (right_leaf->num_keys - idx) * sizeof(uint32_t));
        memmove(&right_leaf->values[idx + 1], &right_leaf->values[idx], (right_leaf->num_keys - idx) * 16);
        
        right_leaf->keys[idx] = key;
        right_leaf->val_lens[idx] = val_len > 16 ? 16 : val_len;
        memcpy(right_leaf->values[idx], value, right_leaf->val_lens[idx]);
        right_leaf->num_keys++;
    } else {
        int idx = leaf_find_key_slot(left_leaf, key);
        memmove(&left_leaf->keys[idx + 1], &left_leaf->keys[idx], (left_leaf->num_keys - idx) * sizeof(uint64_t));
        memmove(&left_leaf->val_lens[idx + 1], &left_leaf->val_lens[idx], (left_leaf->num_keys - idx) * sizeof(uint32_t));
        memmove(&left_leaf->values[idx + 1], &left_leaf->values[idx], (left_leaf->num_keys - idx) * 16);
        
        left_leaf->keys[idx] = key;
        left_leaf->val_lens[idx] = val_len > 16 ? 16 : val_len;
        memcpy(left_leaf->values[idx], value, left_leaf->val_lens[idx]);
        left_leaf->num_keys++;
    }

    uint64_t right_page_id = tree->pager->total_pages;
    right_leaf->next_sibling = left_leaf->next_sibling;
    left_leaf->next_sibling = right_page_id;

    uint8_t internal_buffer[PAGE_SIZE];
    memset(internal_buffer, 0, PAGE_SIZE);
    btree_internal_t *new_root = (btree_internal_t*)internal_buffer;
    new_root->type = NODE_INTERNAL;
    new_root->num_keys = 1;
    new_root->keys[0] = right_leaf->keys[0];
    
    uint64_t left_page_new_id = right_page_id + 1;
    new_root->child_pointers[0] = left_page_new_id;
    new_root->child_pointers[1] = right_page_id;

    pager_write_page(tree->pager, root_id, internal_buffer);
    pager_write_page(tree->pager, left_page_new_id, left_leaf);
    pager_write_page(tree->pager, right_page_id, right_leaf);
    pager_sync(tree->pager);

}

int btree_insert(btree_t *tree, uint64_t key, const uint8_t *value, uint32_t val_len) {
    uint8_t buffer[PAGE_SIZE];
    if (pager_read_page(tree->pager, tree->root_page_id, buffer) < 0) return -1;

    uint8_t *node_ptr = buffer;
    if (*node_ptr == NODE_LEAF) {
        btree_leaf_t *leaf = (btree_leaf_t*)buffer;
        int idx = leaf_find_key_slot(leaf, key);

        if (idx < leaf->num_keys && leaf->keys[idx] == key) {
            leaf->val_lens[idx] = val_len > 16 ? 16 : val_len;
            memcpy(leaf->values[idx], value, leaf->val_lens[idx]);
            return pager_write_page(tree->pager, tree->root_page_id, leaf);
        }

        if (leaf->num_keys >= MAX_KEYS_LEAF) {
            leaf_split_and_insert(tree, tree->root_page_id, leaf, key, value, val_len);
            return 0;
        }

        memmove(&leaf->keys[idx + 1], &leaf->keys[idx], (leaf->num_keys - idx) * sizeof(uint64_t));
        memmove(&leaf->val_lens[idx + 1], &leaf->val_lens[idx], (leaf->num_keys - idx) * sizeof(uint32_t));
        memmove(&leaf->values[idx + 1], &leaf->values[idx], (leaf->num_keys - idx) * 16);

        leaf->keys[idx] = key;
        leaf->val_lens[idx] = val_len > 16 ? 16 : val_len;
        memcpy(leaf->values[idx], value, leaf->val_lens[idx]);
        leaf->num_keys++;

        pager_write_page(tree->pager, tree->root_page_id, leaf);
        pager_sync(tree->pager);
        return 0;
    } else {
        btree_internal_t *internal = (btree_internal_t*)buffer;
        uint64_t target_child = internal->child_pointers[0];
        
        uint8_t child_buf[PAGE_SIZE];
        pager_read_page(tree->pager, target_child, child_buf);
        btree_leaf_t *child_leaf = (btree_leaf_t*)child_buf;
        
        int idx = leaf_find_key_slot(child_leaf, key);
        if (child_leaf->num_keys >= MAX_KEYS_LEAF) {
             leaf_split_and_insert(tree, target_child, child_leaf, key, value, val_len);
             return 0;
        }
        
        memmove(&child_leaf->keys[idx + 1], &child_leaf->keys[idx], (child_leaf->num_keys - idx) * sizeof(uint64_t));
        memmove(&child_leaf->val_lens[idx + 1], &child_leaf->val_lens[idx], (child_leaf->num_keys - idx) * sizeof(uint32_t));
        memmove(&child_leaf->values[idx + 1], &child_leaf->values[idx], (child_leaf->num_keys - idx) * 16);
        child_leaf->keys[idx] = key;
        child_leaf->val_lens[idx] = val_len > 16 ? 16 : val_len;
        memcpy(child_leaf->values[idx], value, child_leaf->val_lens[idx]);
        child_leaf->num_keys++;
        pager_write_page(tree->pager, target_child, child_leaf);
        pager_sync(tree->pager);
        return 0;
    }
    return -1;
}

bool btree_get(btree_t *tree, uint64_t key, uint8_t *val_out, uint32_t *len_out){
  uint8_t buffer[PAGE_SIZE];
  if (pager_read_page(tree->pager, tree->root_page_id, buffer) < 0) return false;

  uint8_t *type = buffer;
  btree_leaf_t *leaf = (btree_leaf_t*)buffer;

  if (*type == NODE_INTERNAL){
    btree_internal_t *internal = (btree_internal_t*)buffer;
    if (pager_read_page(tree->pager, internal->child_pointers[0], buffer)<0) return false;
    leaf = (btree_leaf_t*)buffer;
  }

  int idx = leaf_find_key_slot(leaf, key);
  if (idx < leaf->num_keys && leaf->keys[idx] == key){
    *len_out = leaf->val_lens[idx];
    memcpy(val_out, leaf->values[idx], *len_out);
    return true;
  }
  return false;
}
