#include "stdtype.h"
#include "vmm.h"
#include "pmm.h"

int liballoc_lock() {
    return 0;
}

int liballoc_unlock() {
    return 0;
}

void* liballoc_alloc(int pages) {
    return add_vm_entry(0, pages * PAGE_SIZE, VM_MAP_ANONYMOUS | VM_MAP_WRITE);
}

int liballoc_free(void *ptr, int pages) {
    rm_vm_entry(ptr);
    return (0);
}