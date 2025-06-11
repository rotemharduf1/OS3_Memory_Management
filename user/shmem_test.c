#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE 4096
#define SHARED_ADDR ((char*)0x500000)

int map_shared_pages(void* addr, int size, int pid);
int unmap_shared_pages(void* addr, int size);

int
main(int argc, char *argv[])
{
    char *buf = SHARED_ADDR;
    int shared_size = PGSIZE;

    int child_pid = fork();

    if (child_pid == 0) {
        sleep(10);

        printf("Child: message from parent: %s\n", buf);

        strcpy(buf, "Hello daddy!");
        printf("Child: wrote message\n");

        if (argc < 2 || strcmp(argv[1], "nounmap") != 0) {
            uint64 current_brk = (uint64)sbrk(0);
            uint64 target_end = (uint64)SHARED_ADDR + shared_size;
            if (current_brk < target_end)
                sbrk(target_end - current_brk);

            if (unmap_shared_pages(buf, shared_size) < 0)
                printf("Child: unmap failed\n");
            else
                printf("Child: unmapped successfully\n");
        }

        exit(0);

    } else {
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
        printf("Parent: wrote message\n");

        wait(0);

        printf("Parent: child exited\n");
        printf("Parent: message from child: %s\n", buf);
    }

    exit(0);
}
