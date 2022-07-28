#ifndef _SYSCALL_H
#define _SYSCALL_H

#include <stdint.h>

int32_t syscall_handler(uint32_t syscallno, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5);

#endif