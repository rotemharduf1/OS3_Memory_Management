#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

typedef unsigned int   uint32_t;
typedef unsigned short uint16_t;

#define PGSIZE 4096
#define NUM_CHILDREN 4
#define MAX_MSG_LEN 64

// Declare shared memory syscall
int map_shared_pages(void* addr, int size, int pid);

// Align to 4-byte boundary
char* align4(char* ptr) {
    return (char*)(((uint64)ptr + 3) & ~3);
}

// Construct a message like "[child 2] hello!"
void build_message(int index, char* msg) {
    // start with a template
    strcpy(msg, "[child X] hello!");
    msg[7] = '0' + index;  // replace 'X' with digit
}

// Child log writer
void child_log(int index, char* shared_buf) {
    char* ptr = shared_buf;
    char msg[MAX_MSG_LEN];
    build_message(index, msg);
    int len = strlen(msg);

    while ((uint64)(ptr + 4 + len) <= (uint64)(shared_buf + PGSIZE)) {
        uint32_t* header = (uint32_t*)ptr;
        uint32_t new_header = (index << 16) | len;

        if (__sync_val_compare_and_swap(header, 0, new_header) == 0) {
            memcpy(ptr + 4, msg, len);
            return;
        }

        ptr += 4 + len;
        ptr = align4(ptr);
    }

    printf("Child %d: buffer full, couldn't log\n", index);
}

// Parent log reader
void read_logs(char* shared_buf) {
    char* ptr = shared_buf;

    while ((uint64)(ptr + 4) <= (uint64)(shared_buf + PGSIZE)) {
        uint32_t header = *(uint32_t*)ptr;
        if (header == 0)
            break;

        int index = header >> 16;
        int len = header & 0xFFFF;

        if (len == 0 || len > MAX_MSG_LEN)
            break;

        char msg[MAX_MSG_LEN + 1];
        memcpy(msg, ptr + 4, len);
        msg[len] = '\0';

        printf("Parent: message from child %d: %s\n", index, msg);

        ptr += 4 + len;
        ptr = align4(ptr);
    }
}

// Main function
int main(int argc, char *argv[]) {
    char* shared_buf = malloc(PGSIZE);
    if (!shared_buf) {
        printf("Parent: malloc failed\n");
        exit(1);
    }
    memset(shared_buf, 0, PGSIZE);

    for (int i = 0; i < NUM_CHILDREN; i++) {
        int pid = fork();
        if (pid == 0) {
            sleep(10); // Wait for parent to map
            child_log(i, shared_buf);
            exit(0);
        } else if (pid > 0) {
            if (map_shared_pages(shared_buf, PGSIZE, pid) < 0) {
                printf("Parent: failed to map to child %d\n", pid);
            }
        } else {
            printf("Parent: fork failed\n");
            exit(1);
        }
    }

    for (int i = 0; i < NUM_CHILDREN; i++)
        wait(0);

    printf("\nParent: all children exited. Reading logs:\n");
    read_logs(shared_buf);

    exit(0);
}
