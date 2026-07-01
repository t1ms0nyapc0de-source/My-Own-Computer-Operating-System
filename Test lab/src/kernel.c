#include "gdt.h"
#include "idt.h"
#include "syscall.h"
#include "pmm.h"
#include "vmm.h"
#include "task.h"
#include "timer.h"
#include "vfs.h"
#include "tar.h"
#include "elf.h"
#include "terminal.h"

extern void enter_user_mode(uint32_t entry, uint32_t esp);

void taskA(void) {
    for (int i = 0; i < 5; i++) {
        printf("[Task A] Tick %d\n", i);
        schedule();
    }
    printf("[Task A] Completed.\n");
}

void taskB(void) {
    for (int i = 0; i < 5; i++) {
        printf("[Task B] Tick %d\n", i);
        schedule();
    }
    printf("[Task B] Completed.\n");
}

static uint32_t user_entry = 0;
static uint32_t* user_pd = 0;

void run_user_program(void) {
    printf("[Kernel] Transitioning to Ring 3 User Mode to execute loaded ELF...\n");
    enter_user_mode(user_entry, 0xBFFFFFC);
}

void user_violation_code(void) {
    printf("[User Ring 3] Executing violation code, attempting to write to supervisor page 0xD0000000...\n");
    volatile uint32_t* ptr = (volatile uint32_t*)0xD0000000;
    *ptr = 0xDEADBEEF; // Should trigger page fault with privilege violation
}

void run_supervisor_violation(void) {
    printf("\n[Kernel] Starting Supervisor Protection Test...\n");
    enter_user_mode((uint32_t)user_violation_code, 0xBFFFFFC);
}

void idle_task(void) {
    // Wait for the Task A, Task B, and User ELF to execute and exit
    for (volatile int i = 0; i < 50000000; i++);
    run_supervisor_violation();
    while (1) {
        asm volatile("hlt");
    }
}

void kernel_main(uint32_t magic, struct multiboot_info* mboot_info) {
    terminal_init();
    printf("🪐 MyOS: Custom 32-bit x86 Operating System booting...\n");

    // 1. Setup privilege segments
    gdt_init();
    printf("[GDT] Initialized segments and loaded TSS.\n");

    // 2. Setup Interrupt/Trap Vectors
    idt_init();
    printf("[IDT] Initialized interrupt gates.\n");

    // 3. Setup Syscall Handlers
    syscall_init();
    printf("[Syscall] Registered vector 128 (0x80) handler.\n");

    // 4. Detect Memory & Setup Bitmap
    pmm_init(mboot_info);
    printf("[PMM] Initialized page frame allocator.\n");

    // --- PMM Diagnostics ---
    printf("\n--- Running PMM Diagnostics ---\n");
    uint32_t p1 = pmm_alloc_block();
    printf("[PMM] Allocated block 1: %x\n", p1);
    if (p1 % 4096 != 0) {
        printf("[PMM] Error: Block 1 is not 4KB aligned!\n");
    }
    uint32_t p2 = pmm_alloc_block();
    printf("[PMM] Allocated block 2: %x\n", p2);
    pmm_free_block(p1);
    printf("[PMM] Freed block 1.\n");
    uint32_t p3 = pmm_alloc_block();
    printf("[PMM] Allocated block 3 (should reuse block 1 address): %x\n", p3);
    if (p3 == p1) {
        printf("[PMM] Success: Block reuse verified via first-fit logic.\n");
    } else {
        printf("[PMM] Warning: Block reuse address mismatch: %x vs %x\n", p3, p1);
    }
    pmm_free_block(p2);
    pmm_free_block(p3);

    // 5. Setup Page Directory / Enable Paging
    vmm_init();
    printf("[VMM] Enabled 2-level paging.\n");

    // --- VMM Verification ---
    printf("\n--- Running VMM Verification ---\n");
    // Test A: Map, Write, Verify, Unmap
    uint32_t test_phys = pmm_alloc_block();
    vmm_map_page(kernel_pd, 0xA0000000, test_phys, VMM_FLAG_PRESENT | VMM_FLAG_WRITE | VMM_FLAG_USER);
    printf("[VMM] Test A: Mapped virtual 0xA0000000 to physical %x\n", test_phys);
    *(volatile uint32_t*)0xA0000000 = 0xCAFEBABE;
    uint32_t valA = *(volatile uint32_t*)0xA0000000;
    printf("[VMM] Test A: Read value from 0xA0000000 = %x\n", valA);
    if (valA == 0xCAFEBABE) {
        printf("[VMM] Test A: Passed!\n");
    } else {
        printf("[VMM] Test A: Failed!\n");
    }
    vmm_unmap_page(kernel_pd, 0xA0000000);
    pmm_free_block(test_phys);

    // Test B: Demand Paging (writing to unmapped 0x40000000)
    printf("[VMM] Test B: Writing to unmapped virtual address 0x40000000...\n");
    *(volatile uint32_t*)0x40000000 = 0x12345678;
    uint32_t valB = *(volatile uint32_t*)0x40000000;
    printf("[VMM] Test B: Read value from 0x40000000 = %x\n", valB);
    if (valB == 0x12345678) {
        printf("[VMM] Test B (Demand Paging): Passed!\n");
    } else {
        printf("[VMM] Test B (Demand Paging): Failed!\n");
    }

    // 6. Init RAM Disk from GRUB Modules
    uint32_t initrd_start = 0;
    uint32_t initrd_end = 0;
    if (mboot_info->flags & (1 << 3) && mboot_info->mods_count > 0) {
        struct multiboot_mod* mods = (struct multiboot_mod*)mboot_info->mods_addr;
        initrd_start = mods[0].mod_start;
        initrd_end = mods[0].mod_end;
    }
    printf("[TAR] Initializing RAM Disk. module start: %x, end: %x\n", initrd_start, initrd_end);
    tar_init(initrd_start, initrd_end);

    // 7. Initialize Thread Queues
    scheduler_init();
    printf("[Scheduler] Multi-threading environment ready.\n");

    // Create parallel kernel tasks
    task_create(taskA, kernel_pd);
    task_create(taskB, kernel_pd);

    // 8. Load User ELF from RAM Disk
    user_entry = elf_load("hello", &user_pd);
    if (user_entry != 0) {
        printf("[ELF] Loaded 'hello' from RAM disk. Entry point: %x\n", user_entry);
        task_create(run_user_program, user_pd);
    } else {
        printf("[ELF] Failed to load 'hello'!\n");
    }

    // Create idle task to run the supervisor violation test after other tasks finish
    task_create(idle_task, kernel_pd);

    // 9. Configure PIT at 100Hz
    timer_init(100);
    printf("[Timer] PIT configured at 100Hz.\n");

    printf("\n[Kernel] Enabling interrupts (STI) and entering preemptive multitasking...\n");
    asm volatile("sti");

    // Kernel idle loop
    while (1) {
        asm volatile("hlt");
    }
}
