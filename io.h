static inline void outb(unsigned short port, unsigned char val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outl(unsigned short port, unsigned int val) {
    asm volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline unsigned int inl(unsigned short port) {
    unsigned int ret;
    asm volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline unsigned short inw(unsigned short port) {
    unsigned short ret;
    asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

static inline void flush_tlb_single(unsigned int addr) {
   asm volatile("invlpg (%0)" ::"r" (addr) : "memory");
}