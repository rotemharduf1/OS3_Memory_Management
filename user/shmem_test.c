#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE 4096
#define SHARED_ADDR ((char*)0x500000)

int map_shared_pages(void* addr, int size, int pid);
int unmap_shared_pages(void* addr, int size);

int main(int argc, char *argv[]) {
    char *buf = SHARED_ADDR;
    int shared_size = PGSIZE;

    int child_pid = fork();

    if (child_pid == 0) {
        // Child process
        uint64 before = (uint64)sbrk(0);
        printf("child – size before mapping: %d\n", (int)before);

        sleep(5);

        uint64 after_map = (uint64)sbrk(0);
        printf("child – size after mapping: %d\n", (int)after_map);

        strcpy(buf, "Hello daddy!");

        if (argc < 2 || strcmp(argv[1], "nounmap") != 0) {
            uint64 target_end = (uint64)SHARED_ADDR + shared_size;
            if (after_map < target_end)
                sbrk(target_end - after_map);

            if (unmap_shared_pages(buf, shared_size) < 0)
                printf("child – unmap failed\n");
            else {
                uint64 after_unmap = (uint64)sbrk(0);
                printf("child – size after unmapping: %d\n", (int)after_unmap);
            }
        }

        // Try malloc to see if memory is usable again
        malloc(1);
        uint64 after_malloc = (uint64)sbrk(0);
        printf("child – size after malloc: %d\n", (int)after_malloc);

        exit(0);
    } else {
        // Parent process
        uint64 current_brk = (uint64)sbrk(0);
        uint64 target_end = (uint64)SHARED_ADDR + shared_size;
        if (current_brk < target_end)
            sbrk(target_end - current_brk);

        memset(buf, 0, shared_size);

        if (map_shared_pages(buf, shared_size, child_pid) == (uint64)-1) {
            printf("Parent: mapping to child failed\n");
            kill(child_pid);
            wait(0);
            exit(1);
        }

        strcpy(buf, "Hi child!");

        wait(0);
        printf("parent – read from shared memory: %s\n", buf);
    }

    exit(0);
}
