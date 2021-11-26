#ifndef __INTERRUPTS__
#define __INTERRUPTS__

typedef void (*interrupt_type)(unsigned int, void *);

void register_interrupt(unsigned int intno, interrupt_type fnc, void *ext);

#endif