# osdev

## Memory Map

* 0x00000000 - 0xC0000000 : Userspace application
* 0xC0000000 - 0xC0200000 : Kernel binnary and data (upper limit may change)
* 0xC0200000 - 0xFFD00000 : Kernel Heap
* 0xFFD00000 - 0xFFFFFFFF : Page mapping
