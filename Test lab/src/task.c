#include "task.h"
#include "pmm.h"
#include "vmm.h"
#include "gdt.h"
#include "terminal.h"
#include <string.h>

extern void switch_task(task_t* prev, task_t* next);
extern uint32_t* kernel_pd;

task_t* current_task = 0;
task_t* ready_queue = 0;
uint32_t next_pid = 1;

task_t* get_current_task(void) {
    return current_task;
}

static void enqueue_task(task_t* t) {
    t->next = 0;
    if (ready_queue == 0) {
        ready_queue = t;
    } else {
        task_t* curr = ready_queue;
        while (curr->next != 0) {
            curr = curr->next;
        }
        curr->next = t;
    }
}

static task_t* dequeue_task(void) {
    if (ready_queue == 0) return 0;
    task_t* t = ready_queue;
    ready_queue = ready_queue->next;
    t->next = 0;
    return t;
}

void task_kernel_bootstrap(void (*entry)()) {
    asm volatile("sti");
    entry();
    task_exit(0);
}

void scheduler_init(void) {
    uint32_t page = pmm_alloc_block();
    task_t* boot_task = (task_t*)page;
    boot_task->pid = next_pid++;
    boot_task->state = TASK_RUNNING;
    boot_task->cr3 = (uint32_t)kernel_pd;
    boot_task->kstack = page + PAGE_SIZE;
    boot_task->esp = 0;
    boot_task->next = 0;

    current_task = boot_task;
}

task_t* task_create(void (*entry)(), uint32_t* pd) {
    uint32_t page = pmm_alloc_block();
    memset((void*)page, 0, PAGE_SIZE);

    task_t* t = (task_t*)page;
    t->pid = next_pid++;
    t->state = TASK_READY;
    t->cr3 = (uint32_t)pd;
    t->kstack = page + PAGE_SIZE;

    uint32_t* stack = (uint32_t*)t->kstack;
    
    // Set up initial context switch stack frame
    stack[-1] = (uint32_t)entry;
    stack[-2] = 0;
    stack[-3] = (uint32_t)task_kernel_bootstrap;
    stack[-4] = 0; // EBP
    stack[-5] = 0; // EBX
    stack[-6] = 0; // ESI
    stack[-7] = 0; // EDI

    t->esp = (uint32_t)&stack[-7];

    uint32_t flags;
    asm volatile("pushfl; popl %0; cli" : "=r"(flags));
    enqueue_task(t);
    asm volatile("pushl %0; popfl" : : "r"(flags));

    return t;
}

void schedule(void) {
    uint32_t flags;
    asm volatile("pushfl; popl %0; cli" : "=r"(flags));

    if (ready_queue == 0) {
        asm volatile("pushl %0; popfl" : : "r"(flags));
        return;
    }

    task_t* prev = current_task;
    if (prev->state == TASK_RUNNING) {
        prev->state = TASK_READY;
        enqueue_task(prev);
    }

    task_t* next = dequeue_task();
    next->state = TASK_RUNNING;
    current_task = next;

    switch_task(prev, next);

    asm volatile("pushl %0; popfl" : : "r"(flags));
}

void task_exit(int code) {
    asm volatile("cli");
    printf("[Scheduler] Task %d exited with code %d\n", current_task->pid, code);
    current_task->state = TASK_ZOMBIE;
    schedule();
}
