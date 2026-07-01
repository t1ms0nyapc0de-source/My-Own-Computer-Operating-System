#include "gdt.h"
#include <string.h>

struct gdt_entry gdt[6];
struct gdt_ptr gp;
struct tss_entry tss;

extern void gdt_flush(uint32_t gdt_ptr_addr);

static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;

    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;

    gdt[num].granularity |= gran & 0xF0;
    gdt[num].access = access;
}

void gdt_init(void) {
    gp.limit = (sizeof(struct gdt_entry) * 6) - 1;
    gp.base = (uint32_t)&gdt;

    // 1. Null Descriptor
    gdt_set_gate(0, 0, 0, 0, 0);
    // 2. Kernel Code: base 0, limit 4GB, Ring 0, Exec/Read
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    // 3. Kernel Data: base 0, limit 4GB, Ring 0, Read/Write
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    // 4. User Code: base 0, limit 4GB, Ring 3, Exec/Read
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    // 5. User Data: base 0, limit 4GB, Ring 3, Read/Write
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    // 6. TSS
    memset(&tss, 0, sizeof(struct tss_entry));
    tss.ss0 = 0x10; // Kernel Data segment is 0x10
    tss.esp0 = 0;   // To be updated by scheduler
    tss.iomap_base = sizeof(struct tss_entry);

    uint32_t tss_base = (uint32_t)&tss;
    uint32_t tss_limit = sizeof(struct tss_entry) - 1;
    gdt_set_gate(5, tss_base, tss_limit, 0x89, 0x40);

    gdt_flush((uint32_t)&gp);
}

void tss_set_kernel_stack(uint32_t stack) {
    tss.esp0 = stack;
}
