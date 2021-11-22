extern void gdt_set_gate(int num, unsigned int base, unsigned int limit, unsigned char access, unsigned char gran);
extern void *memset(void* dst, int c, unsigned long size);

struct tss_entry {
	unsigned int prev_tss; // The previous TSS - with hardware task switching these form a kind of backward linked list.
	unsigned int esp0;     // The stack pointer to load when changing to kernel mode.
	unsigned int ss0;      // The stack segment to load when changing to kernel mode.
	// Everything below here is unused.
	unsigned int esp1; // esp and ss 1 and 2 would be used when switching to rings 1 or 2.
	unsigned int ss1;
	unsigned int esp2;
	unsigned int ss2;
	unsigned int cr3;
	unsigned int eip;
	unsigned int eflags;
	unsigned int eax;
	unsigned int ecx;
	unsigned int edx;
	unsigned int ebx;
	unsigned int esp;
	unsigned int ebp;
	unsigned int esi;
	unsigned int edi;
	unsigned int es;
	unsigned int cs;
	unsigned int ss;
	unsigned int ds;
	unsigned int fs;
	unsigned int gs;
	unsigned int ldt;
	unsigned short trap;
	unsigned short iomap_base;
} __attribute__((packed));

struct tss_entry tss;

void write_tss(unsigned short ss0, unsigned short esp0) {
    gdt_set_gate(5, (unsigned int)&tss, (unsigned int)&tss + sizeof(struct tss_entry), 0xE9, 0x00);

    memset(&tss, 0, sizeof(struct tss_entry));
    tss.ss0 = ss0;
    tss.esp0 = esp0;
	tss.cs = 0x0b;
    tss.ss = 0x13;
    tss.ds = 0x13;
    tss.es = 0x13;
    tss.fs = 0x13;
    tss.gs = 0x13;
}