#include "vmm.h"
#include "pmm.h"
#include "terminal.h"
#include <string.h>

uint32_t* kernel_pd = 0;
extern uint32_t pmm_total_blocks;

static inline void invalidate_page(uint32_t va) {
    asm volatile("invlpg (%0)" : : "r"(va) : "memory");
}

void vmm_map_page(uint32_t* pd, uint32_t va, uint32_t pa, uint32_t flags) {
    uint32_t dir_idx = PAGE_DIR_INDEX(va);
    uint32_t tab_idx = PAGE_TAB_INDEX(va);

    // If this is a user process page directory and the page table is currently
    // shared with the kernel directory, we must clone the page table first
    if (pd != kernel_pd && kernel_pd != 0 && pd[dir_idx] == kernel_pd[dir_idx] && pd[dir_idx] != 0) {
        uint32_t old_pt_phys = pd[dir_idx] & 0xFFFFF000;
        uint32_t new_pt_phys = pmm_alloc_block();
        uint32_t* old_pt = (uint32_t*)old_pt_phys;
        uint32_t* new_pt = (uint32_t*)new_pt_phys;
        for (int i = 0; i < 1024; i++) {
            new_pt[i] = old_pt[i];
        }
        pd[dir_idx] = new_pt_phys | (pd[dir_idx] & 0xFFF);
    }

    uint32_t pde = pd[dir_idx];
    uint32_t* pt;

    if (!(pde & VMM_FLAG_PRESENT)) {
        uint32_t pt_phys = pmm_alloc_block();
        pt = (uint32_t*)pt_phys;
        memset(pt, 0, PAGE_SIZE);
        pd[dir_idx] = pt_phys | VMM_FLAG_PRESENT | VMM_FLAG_WRITE | VMM_FLAG_USER;
    } else {
        pt = (uint32_t*)(pde & 0xFFFFF000);
    }

    pt[tab_idx] = (pa & 0xFFFFF000) | flags;
    invalidate_page(va);
}

void vmm_unmap_page(uint32_t* pd, uint32_t va) {
    uint32_t dir_idx = PAGE_DIR_INDEX(va);
    uint32_t tab_idx = PAGE_TAB_INDEX(va);

    // Duplicate table if shared
    if (pd != kernel_pd && kernel_pd != 0 && pd[dir_idx] == kernel_pd[dir_idx] && pd[dir_idx] != 0) {
        uint32_t old_pt_phys = pd[dir_idx] & 0xFFFFF000;
        uint32_t new_pt_phys = pmm_alloc_block();
        uint32_t* old_pt = (uint32_t*)old_pt_phys;
        uint32_t* new_pt = (uint32_t*)new_pt_phys;
        for (int i = 0; i < 1024; i++) {
            new_pt[i] = old_pt[i];
        }
        pd[dir_idx] = new_pt_phys | (pd[dir_idx] & 0xFFF);
    }

    uint32_t pde = pd[dir_idx];
    if (pde & VMM_FLAG_PRESENT) {
        uint32_t* pt = (uint32_t*)(pde & 0xFFFFF000);
        pt[tab_idx] = 0;
        invalidate_page(va);
    }
}

uint32_t* vmm_clone_kernel_directory(void) {
    uint32_t* proc_pd = (uint32_t*)pmm_alloc_block();
    memset(proc_pd, 0, PAGE_SIZE);
    for (int i = 0; i < 1024; i++) {
        proc_pd[i] = kernel_pd[i];
    }
    return proc_pd;
}

void vmm_init(void) {
    kernel_pd = (uint32_t*)pmm_alloc_block();
    memset(kernel_pd, 0, PAGE_SIZE);

    // Identity-map physical memory
    uint32_t mem_end = pmm_total_blocks * PAGE_SIZE;
    for (uint32_t addr = 0; addr < mem_end; addr += PAGE_SIZE) {
        vmm_map_page(kernel_pd, addr, addr, VMM_FLAG_PRESENT | VMM_FLAG_WRITE | VMM_FLAG_USER);
    }

    // Map supervisor-protected test page at 0xD0000000
    uint32_t prot_phys = pmm_alloc_block();
    vmm_map_page(kernel_pd, 0xD0000000, prot_phys, VMM_FLAG_PRESENT | VMM_FLAG_WRITE); // Supervisor only!

    register_interrupt_handler(14, page_fault_handler);

    // Load kernel directory to CR3 and enable paging
    asm volatile("mov %0, %%cr3" : : "r"(kernel_pd));
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" : : "r"(cr0));
}

void page_fault_handler(registers_t* r) {
    uint32_t fault_addr;
    asm volatile("mov %%cr2, %0" : "=r"(fault_addr));

    int present = r->err_code & 0x1;
    int write = r->err_code & 0x2;
    int user = r->err_code & 0x4;

    // Privilege violation check on supervisor page
    if (fault_addr == 0xD0000000) {
        printf("\n[VMM] PRIVILEGE VIOLATION! Access to supervisor page at %x denied.\n", fault_addr);
        printf("EIP: %x, Error Code: %x (Present: %d, Write: %d, User: %d)\n", r->eip, r->err_code, present, write, user);
        printf("Halting system.\n");
        while(1) {
            asm volatile("cli; hlt");
        }
    }

    // Demand paging check
    if (!present && fault_addr >= 0x1000 && fault_addr < 0xC0000000) {
        uint32_t phys = pmm_alloc_block();
        uint32_t active_pd;
        asm volatile("mov %%cr3, %0" : "=r"(active_pd));
        vmm_map_page((uint32_t*)active_pd, fault_addr & 0xFFFFF000, phys, VMM_FLAG_PRESENT | VMM_FLAG_WRITE | VMM_FLAG_USER);
        printf("[VMM] DEMAND PAGING: Mapped virtual %x to physical %x\n", fault_addr & 0xFFFFF000, phys);
        return;
    }

    printf("\nKERNEL PANIC: Page Fault at %x\n", fault_addr);
    printf("EIP: %x, Error Code: %x (Present: %d, Write: %d, User: %d)\n", r->eip, r->err_code, present, write, user);
    while(1) {
        asm volatile("cli; hlt");
    }
}
