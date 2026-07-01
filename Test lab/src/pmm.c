#include "pmm.h"
#include "terminal.h"
#include <string.h>

uint32_t* pmm_bitmap = 0;
uint32_t  pmm_total_blocks = 0;

static inline void pmm_set_block(uint32_t block) {
    pmm_bitmap[block / 32] |= (1 << (block % 32));
}

static inline void pmm_clear_block(uint32_t block) {
    pmm_bitmap[block / 32] &= ~(1 << (block % 32));
}

static inline int pmm_test_block(uint32_t block) {
    return (pmm_bitmap[block / 32] & (1 << (block % 32))) != 0;
}

void pmm_reserve_region(uint32_t start, uint32_t size) {
    uint32_t align_start = start / PAGE_SIZE;
    uint32_t num_blocks = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t i = 0; i < num_blocks; i++) {
        if (align_start + i < pmm_total_blocks) {
            pmm_set_block(align_start + i);
        }
    }
}

void pmm_free_region(uint32_t start, uint32_t size) {
    uint32_t align_start = start / PAGE_SIZE;
    uint32_t num_blocks = size / PAGE_SIZE;
    for (uint32_t i = 0; i < num_blocks; i++) {
        if (align_start + i < pmm_total_blocks) {
            pmm_clear_block(align_start + i);
        }
    }
}

void pmm_init(struct multiboot_info* mboot_info) {
    extern uint32_t _kernel_end;
    uint32_t mem_size = 128 * 1024 * 1024; // Default to 128MB
    if (mboot_info->flags & 1) {
        mem_size = (mboot_info->mem_lower + mboot_info->mem_upper) * 1024 + 1024 * 1024;
    }

    pmm_total_blocks = mem_size / PAGE_SIZE;

    // Dynamically calculate a safe start address for the bitmap after kernel and all modules
    uint32_t bitmap_start = (uint32_t)&_kernel_end;
    if (mboot_info->flags & (1 << 3) && mboot_info->mods_count > 0) {
        struct multiboot_mod* mods = (struct multiboot_mod*)mboot_info->mods_addr;
        for (uint32_t i = 0; i < mboot_info->mods_count; i++) {
            if (mods[i].mod_end > bitmap_start) {
                bitmap_start = mods[i].mod_end;
            }
        }
    }
    // Align to 4KB page boundary
    bitmap_start = (bitmap_start + 4095) & ~4095;
    pmm_bitmap = (uint32_t*)bitmap_start;

    uint32_t num_words = pmm_total_blocks / 32;
    if (pmm_total_blocks % 32 != 0) num_words++;

    // Mark all memory as reserved (1) initially
    memset(pmm_bitmap, 0xFF, num_words * sizeof(uint32_t));

    // Parse memory map if available to free usable RAM
    if (mboot_info->flags & (1 << 6)) {
        struct multiboot_mmap_entry* mmap = (struct multiboot_mmap_entry*)mboot_info->mmap_addr;
        uint32_t mmap_end = mboot_info->mmap_addr + mboot_info->mmap_length;
        while ((uint32_t)mmap < mmap_end) {
            if (mmap->type == 1) { // Usable RAM
                pmm_free_region((uint32_t)mmap->addr, (uint32_t)mmap->len);
            }
            mmap = (struct multiboot_mmap_entry*)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
        }
    } else {
        // Fallback: free 1MB to end of detected memory
        pmm_free_region(0x100000, mem_size - 0x100000);
    }

    // Reserve critical zones:
    // 1. BIOS/VGA (first 1MB)
    pmm_reserve_region(0x0, 0x100000);

    // 2. Kernel image
    pmm_reserve_region(0x100000, (uint32_t)&_kernel_end - 0x100000);

    // 3. Multiboot modules (initrd)
    if (mboot_info->flags & (1 << 3) && mboot_info->mods_count > 0) {
        struct multiboot_mod* mods = (struct multiboot_mod*)mboot_info->mods_addr;
        for (uint32_t i = 0; i < mboot_info->mods_count; i++) {
            uint32_t mod_start = mods[i].mod_start;
            uint32_t mod_end = mods[i].mod_end;
            pmm_reserve_region(mod_start, mod_end - mod_start);
        }
    }

    // 4. The PMM bitmap itself
    pmm_reserve_region(bitmap_start, num_words * sizeof(uint32_t));
}

uint32_t pmm_alloc_block(void) {
    uint32_t num_words = pmm_total_blocks / 32;
    for (uint32_t i = 0; i < num_words; i++) {
        if (pmm_bitmap[i] != 0xFFFFFFFF) {
            for (int bit = 0; bit < 32; bit++) {
                if (!(pmm_bitmap[i] & (1 << bit))) {
                    uint32_t block = i * 32 + bit;
                    pmm_set_block(block);
                    return block * PAGE_SIZE;
                }
            }
        }
    }
    printf("KERNEL PANIC: Out of physical memory!\n");
    while(1) {
        asm volatile("cli; hlt");
    }
}

void pmm_free_block(uint32_t addr) {
    uint32_t block = addr / PAGE_SIZE;
    if (block < pmm_total_blocks) {
        pmm_clear_block(block);
    }
}
