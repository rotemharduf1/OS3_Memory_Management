#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE 4096
#define HEADER_SIZE 4
#define MAX_CHILDREN 7

// user syscall
int map_shared_pages(void* addr, int size, int pid);

void make_msg(char *buf, int child_index, int num) {
  char numbuf[16];
  char *p = buf;

  strcpy(p, "Child ");
  p += strlen(p);
  *p++ = '0' + child_index;

  strcpy(p, " msg 0 says hello #");
  p += strlen(p);

  int i = 0;
  do {
    numbuf[i++] = '0' + (num % 10);
    num /= 10;
  } while (num > 0);

  while (i-- > 0) {
    *p++ = numbuf[i];
  }

  *p = '\0';
}

void write_log(char *buffer, int child_index, int num) {
  char msg[64];
  make_msg(msg, child_index, num);
  int len = strlen(msg);
  int i = 0;

  while (i + HEADER_SIZE + len <= PGSIZE) {
    i = (i + 3) & ~3; // Align to 4 bytes

    if (i + HEADER_SIZE + len > PGSIZE)
      break;

    uint32 *header = (uint32*)(buffer + i);
    uint32 new_header = (child_index << 16) | len;

    if (__sync_val_compare_and_swap(header, 0, new_header) == 0) {
      memmove((char*)header + HEADER_SIZE, msg, len);
      break;
    } else {
      int old_len = (*header) & 0xFFFF;
      i += HEADER_SIZE + old_len;
    }
  }
}

void read_logs(char *buffer) {
  char *p = buffer;

  while ((p - buffer) + HEADER_SIZE <= PGSIZE) {
    p = (char*)(((uint64)p + 3) & ~3); // Align
    uint32 header = *(uint32*)p;

    if (header == 0)
      break;

    int index = header >> 16;
    int len = header & 0xFFFF;

    printf("From child %d: \"", index);
    write(1, p + HEADER_SIZE, len);
    printf("\"\n");

    p += HEADER_SIZE + len;
  }
}

int main(int argc, char *argv[]) {
  char *shared = (char*) sbrk(PGSIZE);
  memset(shared, 0, PGSIZE);

  for (int i = 0; i < MAX_CHILDREN; i++) {
    int pid = fork();
    if (pid == 0) {
      sleep(5);  // Let parent finish mapping
      write_log(shared, i, (i + 1) * 2726);  // Unique number per child
      exit(0);
    } else {
      if (map_shared_pages(shared, PGSIZE, pid) == (uint64)-1) {
        printf("Mapping to child %d failed\n", i);
        kill(pid);
      }
    }
  }

  for (int i = 0; i < MAX_CHILDREN; i++)
    wait(0);

  read_logs(shared);
  exit(0);
}
