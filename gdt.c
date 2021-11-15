
extern void load_gdt(void);

struct gdt_entry {
    unsigned short limit_low;           // The lower 16 bits of the limit.
    unsigned short base_low;            // The lower 16 bits of the base.
    unsigned char  base_middle;         // The next 8 bits of the base.
    unsigned char  access;              // Access flags, determine what ring this segment can be used in.
    unsigned char  granularity;
    unsigned char  base_high;           // The last 8 bits of the base.
} __attribute__((packed));

struct gdt_ptr {
    unsigned short limit;               // The upper 16 bits of all selector limits.
    unsigned int base;                // The address of the first gdt_entry_t struct.
} __attribute__((packed));

#define GDT_ENTRIES_SZ 3
#define GDT_CS_KERN 1
#define GDT_DS_KERN 2

struct gdt_entry gdt_entries[GDT_ENTRIES_SZ];
struct gdt_ptr gdt_base;

void gdt_set_gate(int num, unsigned int base, unsigned int limit, unsigned char access, unsigned char gran) {
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;
    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;
    gdt_entries[num].granularity |= gran & 0xF0;
    gdt_entries[num].access      = access;
}

void setup_gdt() {
    gdt_set_gate(0, 0, 0, 0, 0);                // Null segment 0x00
    gdt_set_gate(GDT_CS_KERN, 0, 0xFFFFFFFF, 0x9A, 0xCF); // Code segment 0x08
    gdt_set_gate(GDT_DS_KERN, 0, 0xFFFFFFFF, 0x92, 0xCF); // Data segment 0x10

    gdt_base.base = (unsigned int)&gdt_entries;
    gdt_base.limit = sizeof(struct gdt_entry) * GDT_ENTRIES_SZ - 1;

    load_gdt();    
}
