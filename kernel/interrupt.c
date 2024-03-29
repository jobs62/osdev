#include "io.h"
#include "interrupt.h"
#include "vmm.h"
#include <stdint.h>
#include "syscall.h"

struct cpu_state {
    unsigned int edi;
    unsigned int esi;
    unsigned int ebp;
    unsigned int esp; //pushed by pusha but ignored by popa
    unsigned int ebx;
    unsigned int edx;
    unsigned int ecx;
    unsigned int eax;
} __attribute__((packed));

struct stack_state {
    unsigned int error_code;
    unsigned int eip;
    unsigned int cs;
    unsigned int eflags;
    unsigned int esp; //only pushed/poped if ring3 -> ring0 int
    unsigned int ss; //only pushed/poped if ring3 -> ring0 int
} __attribute__((packed));

struct idt_entry {
    unsigned short base_lo;             // The lower 16 bits of the address to jump to when this interrupt fires.
    unsigned short sel;                 // Kernel segment selector.
    unsigned char  always0;             // This must always be zero.
    unsigned char  flags;               // More flags. See documentation.
    unsigned short base_hi;             // The upper 16 bits of the address to jump to.
} __attribute__((packed));

struct idt_ptr {
    unsigned short limit;
    unsigned int base; 
} __attribute__((packed));

#define IDT_TABLE_SZ 256
struct idt_entry idt_entries[IDT_TABLE_SZ];
struct idt_ptr idt_base;

extern void interrupt_handler_0(void);
extern void interrupt_handler_1(void);
extern void interrupt_handler_2(void);
extern void interrupt_handler_3(void);
extern void interrupt_handler_4(void);
extern void interrupt_handler_5(void);
extern void interrupt_handler_6(void);
extern void interrupt_handler_7(void);
extern void interrupt_handler_8(void);
extern void interrupt_handler_9(void);
extern void interrupt_handler_10(void);
extern void interrupt_handler_11(void);
extern void interrupt_handler_12(void);
extern void interrupt_handler_13(void);
extern void interrupt_handler_14(void);
extern void interrupt_handler_15(void);
extern void interrupt_handler_16(void);
extern void interrupt_handler_17(void);
extern void interrupt_handler_18(void);
extern void interrupt_handler_19(void);
extern void interrupt_handler_20(void);
extern void interrupt_handler_21(void);
extern void interrupt_handler_22(void);
extern void interrupt_handler_23(void);
extern void interrupt_handler_24(void);
extern void interrupt_handler_25(void);
extern void interrupt_handler_26(void);
extern void interrupt_handler_27(void);
extern void interrupt_handler_28(void);
extern void interrupt_handler_29(void);
extern void interrupt_handler_30(void);
extern void interrupt_handler_31(void);
extern void interrupt_handler_32(void);
extern void interrupt_handler_33(void);
extern void interrupt_handler_34(void);
extern void interrupt_handler_35(void);
extern void interrupt_handler_36(void);
extern void interrupt_handler_37(void);
extern void interrupt_handler_38(void);
extern void interrupt_handler_39(void);
extern void interrupt_handler_40(void);
extern void interrupt_handler_41(void);
extern void interrupt_handler_42(void);
extern void interrupt_handler_43(void);
extern void interrupt_handler_44(void);
extern void interrupt_handler_45(void);
extern void interrupt_handler_46(void);
extern void interrupt_handler_47(void);
extern void interrupt_handler_128(void);

struct inter_holder {
    unsigned char present;
    interrupt_type fnc;
    void *ext;
};

struct inter_holder int_reg[48];

void PIC_sendEOI(unsigned char irq);
void PIC_remap(int offset1, int offset2);

#define PIC1 0x20
#define PIC2 0xA0
#define PIC1_COMMAND PIC1
#define PIC2_COMMAND PIC2
#define PIC1_DATA (PIC1+1)
#define PIC2_DATA (PIC2+1)

#define PIC_EOI 0x20 //End of interrupt command

struct fullstack {
    unsigned int cr3;
    struct cpu_state cpu;
    unsigned int interrupt;
    struct stack_state stack;
};

void interrupt_handler(struct fullstack *fstack) {
    if (fstack->interrupt < 48 && int_reg[fstack->interrupt].present == 1) {
        int_reg[fstack->interrupt].fnc(fstack->interrupt, int_reg[fstack->interrupt].ext);
    } else if(fstack->interrupt == 128) {
        fstack->cpu.eax = syscall_handler(fstack->cpu.eax, fstack->cpu.ebx, fstack->cpu.ecx, fstack->cpu.edx, fstack->cpu.esi, fstack->cpu.edi);
    } else {
        kprintf("CS=0x%8h, int_no=0x%8h, err_code=0x%8h\n", fstack->stack.cs, fstack->interrupt, fstack->stack.error_code);
        kprintf("EDI=0x%8h, ESI=0x%8h, EBP=0x%8h\n", fstack->cpu.edi, fstack->cpu.esi, fstack->cpu.ebp);
        kprintf("ESP=0x%8h, EBX=0x%8h, EDX=0x%8h\n", fstack->cpu.esp, fstack->cpu.ebx, fstack->cpu.edx);
        kprintf("ECX=0x%8h, EAX=0x%8h, EIP=0x%8h\n", fstack->cpu.ecx, fstack->cpu.eax, fstack->stack.eip);
    }

    PIC_sendEOI(fstack->interrupt);
}

void idt_set_gate(unsigned char num, unsigned int base, unsigned short sel, unsigned char flags) {
    idt_entries[num].base_lo = base & 0xFFFF;
    idt_entries[num].base_hi = (base >> 16) & 0xFFFF;
    idt_entries[num].sel = sel;
    idt_entries[num].always0 = 0;
    idt_entries[num].flags = flags | 0x60;
}

#define PIC1_START_INTERRUPT 0x20
#define PIC2_START_INTERRUPT 0x28

void setup_idt() {
    memset(int_reg, 0, sizeof(int_reg));

    idt_set_gate(0, (unsigned int)&interrupt_handler_0, 0x08, 0x8E);     
    idt_set_gate(1, (unsigned int)&interrupt_handler_1, 0x08, 0x8E);     
    idt_set_gate(2, (unsigned int)&interrupt_handler_2, 0x08, 0x8E);     
    idt_set_gate(3, (unsigned int)&interrupt_handler_3, 0x08, 0x8E);     
    idt_set_gate(4, (unsigned int)&interrupt_handler_4, 0x08, 0x8E);     
    idt_set_gate(5, (unsigned int)&interrupt_handler_5, 0x08, 0x8E);     
    idt_set_gate(6, (unsigned int)&interrupt_handler_6, 0x08, 0x8E);     
    idt_set_gate(7, (unsigned int)&interrupt_handler_7, 0x08, 0x8E);     
    idt_set_gate(8, (unsigned int)&interrupt_handler_8, 0x08, 0x8E);     
    idt_set_gate(9, (unsigned int)&interrupt_handler_9, 0x08, 0x8E);     
    idt_set_gate(10, (unsigned int)&interrupt_handler_10, 0x08, 0x8E);     
    idt_set_gate(11, (unsigned int)&interrupt_handler_11, 0x08, 0x8E);     
    idt_set_gate(12, (unsigned int)&interrupt_handler_12, 0x08, 0x8E);     
    idt_set_gate(13, (unsigned int)&interrupt_handler_13, 0x08, 0x8E);     
    idt_set_gate(14, (unsigned int)&interrupt_handler_14, 0x08, 0x8E);     
    idt_set_gate(15, (unsigned int)&interrupt_handler_15, 0x08, 0x8E);     
    idt_set_gate(16, (unsigned int)&interrupt_handler_16, 0x08, 0x8E);     
    idt_set_gate(17, (unsigned int)&interrupt_handler_17, 0x08, 0x8E);     
    idt_set_gate(18, (unsigned int)&interrupt_handler_18, 0x08, 0x8E);     
    idt_set_gate(19, (unsigned int)&interrupt_handler_19, 0x08, 0x8E);     
    idt_set_gate(20, (unsigned int)&interrupt_handler_20, 0x08, 0x8E);     
    idt_set_gate(21, (unsigned int)&interrupt_handler_21, 0x08, 0x8E);     
    idt_set_gate(22, (unsigned int)&interrupt_handler_22, 0x08, 0x8E);     
    idt_set_gate(23, (unsigned int)&interrupt_handler_23, 0x08, 0x8E);     
    idt_set_gate(24, (unsigned int)&interrupt_handler_24, 0x08, 0x8E);     
    idt_set_gate(25, (unsigned int)&interrupt_handler_25, 0x08, 0x8E);     
    idt_set_gate(26, (unsigned int)&interrupt_handler_26, 0x08, 0x8E);     
    idt_set_gate(27, (unsigned int)&interrupt_handler_27, 0x08, 0x8E);     
    idt_set_gate(28, (unsigned int)&interrupt_handler_28, 0x08, 0x8E); 
    idt_set_gate(29, (unsigned int)&interrupt_handler_29, 0x08, 0x8E);   
    idt_set_gate(30, (unsigned int)&interrupt_handler_30, 0x08, 0x8E);  
    idt_set_gate(31, (unsigned int)&interrupt_handler_31, 0x08, 0x8E);
    idt_set_gate(32, (unsigned int)&interrupt_handler_32, 0x08, 0x8E);
    idt_set_gate(33, (unsigned int)&interrupt_handler_33, 0x08, 0x8E);
    idt_set_gate(34, (unsigned int)&interrupt_handler_34, 0x08, 0x8E);
    idt_set_gate(35, (unsigned int)&interrupt_handler_35, 0x08, 0x8E);
    idt_set_gate(36, (unsigned int)&interrupt_handler_36, 0x08, 0x8E);
    idt_set_gate(37, (unsigned int)&interrupt_handler_37, 0x08, 0x8E);
    idt_set_gate(38, (unsigned int)&interrupt_handler_38, 0x08, 0x8E);
    idt_set_gate(39, (unsigned int)&interrupt_handler_39, 0x08, 0x8E);
    idt_set_gate(40, (unsigned int)&interrupt_handler_40, 0x08, 0x8E);
    idt_set_gate(41, (unsigned int)&interrupt_handler_41, 0x08, 0x8E);
    idt_set_gate(42, (unsigned int)&interrupt_handler_42, 0x08, 0x8E);
    idt_set_gate(43, (unsigned int)&interrupt_handler_43, 0x08, 0x8E);
    idt_set_gate(44, (unsigned int)&interrupt_handler_44, 0x08, 0x8E);
    idt_set_gate(45, (unsigned int)&interrupt_handler_45, 0x08, 0x8E);
    idt_set_gate(46, (unsigned int)&interrupt_handler_46, 0x08, 0x8E);
    idt_set_gate(47, (unsigned int)&interrupt_handler_47, 0x08, 0x8E);
    idt_set_gate(128, (unsigned int)&interrupt_handler_128, 0x08, 0x8E);

    idt_base.base = (unsigned int)&idt_entries;
    idt_base.limit = sizeof(struct idt_entry) * IDT_TABLE_SZ - 1;

    PIC_remap(PIC1_START_INTERRUPT, PIC2_START_INTERRUPT); //Remap pic to 0x20 -> 0x28 and 0x28 -> 0x2F to avoid conflicts with cpu exeption

    asm volatile("lidt %0" : : "m" (idt_base));

    outb(PIC1_DATA, 0xff); //mask all
    outb(PIC2_DATA, 0xff);
}

void PIC_sendEOI(unsigned char irq) {
    if (irq < PIC1_START_INTERRUPT || irq >= PIC2_START_INTERRUPT + 7) {
        return; 
    }

    if (irq >= PIC2_START_INTERRUPT) {
        outb(PIC2_COMMAND, PIC_EOI);
    }

    outb(PIC1_COMMAND, PIC_EOI);
}

#define ICW1_ICW4 0x01
#define ICW1_SINGLE 0x02
#define ICW1_INTERVAL4 0x04
#define ICW1_LEVEL 0x08
#define ICW1_INIT 0x10

#define ICW4_8086 0x01
#define ICW4_AUTO 0x02
#define ICW4_BUF_SLAVE 0x08
#define ICW4_BUF_MASTER 0x0C
#define ICW4_SFNM  0x10

void PIC_remap(int offset1, int offset2) {
    unsigned char a1;
    unsigned char a2;

    a1 = inb(PIC1_DATA); //save masks
    a2 = inb(PIC2_DATA);

    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC1_DATA, offset1);
    io_wait();
    outb(PIC2_DATA, offset2);
    io_wait();
    outb(PIC1_DATA, 4);
    io_wait();
    outb(PIC2_DATA, 2);
    io_wait();

    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);

    outb(PIC1_DATA, a1); //restor saved masks
    outb(PIC2_DATA, a2);
}

void register_interrupt(unsigned int intno, interrupt_type fnc, void *ext) {
    int_reg[intno].fnc = fnc;
    int_reg[intno].ext = ext;
    int_reg[intno].present = 1;

    uint16_t port;
    uint8_t value;
    uint8_t irqline;
    if (intno >= PIC1_START_INTERRUPT && intno < PIC2_START_INTERRUPT) {
        port = PIC1_DATA;
        irqline = intno - PIC1_START_INTERRUPT;
    } else if (intno >= PIC2_START_INTERRUPT) {
        port = PIC2_DATA;
        irqline = intno - PIC2_START_INTERRUPT;
    } else {
        return;
    }

    value = inb(port) & ~(1 << irqline);
    outb(port, value);
}