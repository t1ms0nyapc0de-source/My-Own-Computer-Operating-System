#ifndef SYSCALL_H
#define SYSCALL_H

#include "idt.h"

#define SYSCALL_WRITE 0
#define SYSCALL_EXIT  1

void syscall_init(void);
void syscall_dispatch(registers_t *r);

#endif
