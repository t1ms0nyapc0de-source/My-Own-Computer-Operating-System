#include "syscall.h"
#include "terminal.h"

// Forward declaration of task exit function from scheduler
extern void task_exit(int code);

void syscall_dispatch(registers_t *r) {
    uint32_t syscall_num = r->eax;

    if (syscall_num == SYSCALL_WRITE) {
        const char *str = (const char*) r->ecx;
        uint32_t len = r->edx;
        for (uint32_t i = 0; i < len; i++) {
            terminal_putchar(str[i]);
        }
        r->eax = len; // Return value in EAX
    } else if (syscall_num == SYSCALL_EXIT) {
        int exit_code = (int) r->ebx;
        task_exit(exit_code);
    } else {
        printf("Unknown syscall: %d\n", syscall_num);
        r->eax = -1;
    }
}

void syscall_init(void) {
    register_interrupt_handler(128, syscall_dispatch);
}
