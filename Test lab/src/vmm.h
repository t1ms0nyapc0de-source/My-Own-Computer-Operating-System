#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include "idt.h"

#define VMM_FLAG_PRESENT    0x01
#define VMM_FLAG_WRITE      0x02
#define VMM_FLAG_USER       0x04

#define PAGE_DIR_INDEX(vaddr)   (((vaddr) >> 22) & 0x3FF)
#define PAGE_TAB_INDEX(vaddr)   (((vaddr) >> 12) & 0x3FF)
#define PAGE_OFFSET(vaddr)      ((vaddr) & 0xFFF)

extern uint32_t* kernel_pd;

void vmm_init(void);
void vmm_map_page(uint32_t* pd, uint32_t va, uint32_t pa, uint32_t flags);
void vmm_unmap_page(uint32_t* pd, uint32_t va);
uint32_t* vmm_clone_kernel_directory(void);
void page_fault_handler(registers_t* r);

#endif
