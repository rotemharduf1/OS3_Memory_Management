#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE 4096
#define SHARED_ADDR ((char*)0x800000)
#define MAX_CHILDREN 4
#define HEADER_SIZE 4

int map_shared_pages(void* addr, int size, int pid);

void write_log(char *buffer, int child_index, const char *message) {
    int len = strlen(message);
    char *p = buffer;

    while (1) {
        p = (char*)(((uint64)p + 3) & ~3);  // align to 4 bytes

        if ((p + HEADER_SIZE + len) >= buffer + PGSIZE)
            break;

        uint32 *header = (uint32*)p;
        uint32 new_header = (child_index << 16) | len;

        if (__sync_val_compare_and_swap(header, 0, new_header) == 0) {
            memmove(p + HEADER_SIZE, message, len);
            break;
        } else {
            p += HEADER_SIZE + len;
        }
    }

    exit(0);
}

// Parent reads messages from buffer
void read_logs(char *buffer) {
    char *p = buffer;

    while (p < buffer + PGSIZE) {
        p = (char*)(((uint64)p + 3) & ~3);

        uint32 header = *(uint32*)p;
        if (header == 0)
            break;

        int index = header >> 16;
        int len = header & 0xFFFF;

        printf("From child %d: ", index);
        write(1, p + HEADER_SIZE, len);
        write(1, "\n", 1);

        p += HEADER_SIZE + len;
    }
}

int
main(int argc, char *argv[])
{
    char *shared = SHARED_ADDR;

    uint64 current_brk = (uint64)sbrk(0);
    uint64 target = (uint64)shared + PGSIZE;
    if (current_brk < target)
        sbrk(target - current_brk);
    memset(shared, 0, PGSIZE);

    for (int i = 0; i < MAX_CHILDREN; i++) {
        int pid = fork();
        if (pid == 0) {
            sleep(10);
            write_log(shared, i, "Hello from child");
        } else {
            if (map_shared_pages(shared, PGSIZE, pid) == (uint64)-1) {
                printf("Mapping to child %d failed\n", pid);
                kill(pid);
            }
        }
    }

    for (int i = 0; i < MAX_CHILDREN; i++)
        wait(0);

    read_logs(shared);
    exit(0);
}
