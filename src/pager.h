#ifndef PAGER_H
#define PAGER_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096

typedef struct {
  int file_fd;
  uint64_t total_pages;
} pager_t;

pager_t* pager_open(const char *filepath);
int pager_read_page(pager_t *pager, uint64_t page_id, void *page_buffer);
int pager_write_page(pager_t *pager, uint64_t page_id, const void *page_buffer);
int pager_sync(pager_t *pager);
void pager_close(pager_t *pager);

#endif
