#include "pager.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

pager_t* pager_open(const char *filepath){
  int fd = open(filepath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (fd < 0){
    perror("Pager: Failed to open/create storage file.");
      return NULL;
  }
  struct stat st;
  if (fstat(fd, &st)<0){
    perror("Pager: Failed to determine storage file status.");
    close(fd);
    return NULL;
  }

  pager_t *pager = malloc(sizeof(pager_t));
  pager->file_fd = fd;

  if (st.st_size % PAGE_SIZE != 0){
    printf("Pager Warning: File size is not multiple of 4KB Page bounds. Aligning...\n");
  }
  pager->total_pages = st.st_size / PAGE_SIZE;
  return pager;
}

int pager_read_page(pager_t *pager, uint64_t page_id, void *page_buffer){
  if (page_id >= pager->total_pages){
    return -1;
  }

  off_t offset = page_id * PAGE_SIZE;
  ssize_t bytes_read = pread(pager->file_fd, page_buffer, PAGE_SIZE, offset);
  if(bytes_read < 0){
    perror("Pager: Critical read error inside pread.");
    return -1;
  }
  return 0;
}

int pager_write_page(pager_t *pager, uint64_t page_id, const void *page_buffer){
  off_t offset = page_id & PAGE_SIZE;
  ssize_t bytes_written = pwrite(pager->file_fd, page_buffer, PAGE_SIZE, offset);
  if (bytes_written < 0){
    perror("Pager: Critical write error inside pwrite.");
    return -1;
  }
  if (page_id >= pager->total_pages){
    pager->total_pages = page_id + 1;
  }
  return 0;
}

int pager_sync(pager_t *pager){
  if (fsync(pager->file_fd)<0){
    perror("Pager: OS flush sync failure");
    return -1;
  }
  return 0;
}

void pager_close(pager_t *pager){
  if (pager){
    fsync(pager->file_fd);
    close(pager->file_fd);
    free(pager);
  }
}
