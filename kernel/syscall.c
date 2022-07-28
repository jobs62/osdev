#include "syscall.h"
#include <stdint.h>
#include <stddef.h>
#include "vmm.h"

enum {
    SYSCALL_EXIT = 66,
    SYSCALL_WRITE = 42,
};

extern int put(char c);

static int32_t syscall_write(const void *buffer, size_t buffer_sz) {
    if (__check_ptr_userspace(buffer, buffer_sz)) {
        return (-1);
    }

    uint32_t index;
    for (index = 0; index < buffer_sz; index++) {
        put(((char *)buffer)[index]);
    }

    return index;
}

static int32_t syscall_exit(uint32_t code) {
    kprintf("task finished with return code %d\n", code);
    while(1) {}
}

int32_t syscall_handler(uint32_t syscallno, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5) {
    switch (syscallno) {
        case SYSCALL_WRITE:
            return syscall_write((const void *)arg1, (size_t)arg2);
        case SYSCALL_EXIT:
            return syscall_exit(arg1);
    }
}