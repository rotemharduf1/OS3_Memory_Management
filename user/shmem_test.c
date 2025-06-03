#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE 4096

// Declare the syscalls
int map_shared_pages(void* addr, int size, int pid);
int unmap_shared_pages(void* addr, int size);

int
main(int argc, char *argv[])
{
    int shared_size = PGSIZE;
    char *local_buf = malloc(shared_size);
    memset(local_buf, 0, shared_size);

    int parent_pid = getpid();
    printf("Parent: my pid is %d\n", parent_pid);

    char *before = sbrk(0);
    printf("Parent: sz before fork: %p\n", before);

    int child_pid = fork();

    if (child_pid == 0) {
        // ---------------- Child ----------------
        sleep(10); // allow parent to map if needed

        char *sz_before = sbrk(0);
        printf("Child: sz before mapping: %p\n", sz_before);

        int result = map_shared_pages(local_buf, shared_size, parent_pid);
        if (result == -1) {
            printf("Child: mapping failed\n");
            exit(1);
        }

        char *sz_after_map = sbrk(0);
        printf("Child: sz after mapping: %p\n", sz_after_map);

        strcpy(local_buf, "Hello daddy!");
        printf("Child: wrote message\n");

        if (argc < 2 || strcmp(argv[1], "nounmap") != 0) {
            if (unmap_shared_pages(local_buf, shared_size) < 0)
                printf("Child: unmap failed\n");
            else
                printf("Child: unmapped successfully\n");

            char *sz_after_unmap = sbrk(0);
            printf("Child: sz after unmapping: %p\n", sz_after_unmap);

            // malloc test
            char* test = malloc(128);
            strcpy(test, "testing malloc");
            printf("Child: malloc result: %s\n", test);
            free(test);

            char *sz_after_malloc = sbrk(0);
            printf("Child: sz after malloc: %p\n", sz_after_malloc);
        }

        exit(0);

    } else {
        // ---------------- Parent ----------------
        wait(0);
        printf("Parent: child exited\n");
        printf("Parent: message from child: %s\n", local_buf);
    }

    exit(0);
}
