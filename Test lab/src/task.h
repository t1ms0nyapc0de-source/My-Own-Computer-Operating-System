#ifndef TASK_H
#define TASK_H

#include <stdint.h>

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_ZOMBIE
} task_state_t;

typedef struct task {
    uint32_t pid;
    uint32_t esp;      // Current stack pointer (saved esp)
    uint32_t cr3;      // Physical page directory address
    uint32_t kstack;   // Top of kernel stack
    task_state_t state;
    struct task* next;
} task_t;

void scheduler_init(void);
task_t* task_create(void (*entry)(), uint32_t* pd);
void schedule(void);
void task_exit(int code);
task_t* get_current_task(void);

#endif
