#include "elf.h"
#include "vfs.h"
#include "pmm.h"
#include "vmm.h"
#include "terminal.h"
#include <string.h>

extern uint32_t* kernel_pd;

#define EI_NIDENT 16
#define ELF_MAGIC 0x464C457F // "\x7FELF" in little endian

typedef struct {
    uint8_t  e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf32_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} Elf32_Phdr;

uint32_t elf_load(const char* path, uint32_t** out_pd) {
    fs_node_t* file = find_node_by_path(path);
    if (!file) {
        printf("[ELF] File not found: %s\n", path);
        return 0;
    }

    Elf32_Ehdr ehdr;
    if (read_fs(file, 0, sizeof(Elf32_Ehdr), (uint8_t*)&ehdr) != sizeof(Elf32_Ehdr)) {
        printf("[ELF] Failed to read ELF header\n");
        return 0;
    }

    uint32_t* magic = (uint32_t*)ehdr.e_ident;
    if (*magic != ELF_MAGIC) {
        printf("[ELF] Invalid ELF magic: %x\n", *magic);
        return 0;
    }
    if (ehdr.e_machine != 3) { // i386
        printf("[ELF] Unsupported machine: %d\n", ehdr.e_machine);
        return 0;
    }
    if (ehdr.e_type != 2) { // Executable
        printf("[ELF] File is not executable\n");
        return 0;
    }

    // Allocate process page directory (clones kernel mappings)
    uint32_t* proc_pd = vmm_clone_kernel_directory();

    // Temporarily switch CR3 to process page directory to load segments
    uint32_t flags;
    asm volatile("pushfl; popl %0; cli" : "=r"(flags));
    uint32_t original_cr3;
    asm volatile("mov %%cr3, %0" : "=r"(original_cr3));
    asm volatile("mov %0, %%cr3" : : "r"(proc_pd));

    // Load segments
    for (int i = 0; i < ehdr.e_phnum; i++) {
        Elf32_Phdr phdr;
        uint32_t offset = ehdr.e_phoff + i * sizeof(Elf32_Phdr);
        
        if (read_fs(file, offset, sizeof(Elf32_Phdr), (uint8_t*)&phdr) != sizeof(Elf32_Phdr)) {
            printf("[ELF] Failed to read program header %d\n", i);
            asm volatile("mov %0, %%cr3" : : "r"(original_cr3));
            asm volatile("pushl %0; popfl" : : "r"(flags));
            return 0;
        }

        if (phdr.p_type == 1) { // PT_LOAD
            uint32_t vaddr_start = phdr.p_vaddr;
            uint32_t mem_size = phdr.p_memsz;
            uint32_t page_start = vaddr_start & 0xFFFFF000;
            uint32_t page_end = (vaddr_start + mem_size + PAGE_SIZE - 1) & 0xFFFFF000;

            for (uint32_t page_va = page_start; page_va < page_end; page_va += PAGE_SIZE) {
                uint32_t phys = pmm_alloc_block();
                vmm_map_page(proc_pd, page_va, phys, VMM_FLAG_PRESENT | VMM_FLAG_WRITE | VMM_FLAG_USER);
                memset((void*)page_va, 0, PAGE_SIZE);
            }

            // Copy file segment content into memory
            read_fs(file, phdr.p_offset, phdr.p_filesz, (uint8_t*)phdr.p_vaddr);
        }
    }

    // Allocate user space stack page at [0xBFFF0000, 0xC0000000)
    uint32_t stack_phys = pmm_alloc_block();
    vmm_map_page(proc_pd, 0xBFFF0000, stack_phys, VMM_FLAG_PRESENT | VMM_FLAG_WRITE | VMM_FLAG_USER);
    memset((void*)0xBFFF0000, 0, PAGE_SIZE);

    // Switch back to original page directory
    asm volatile("mov %0, %%cr3" : : "r"(original_cr3));
    asm volatile("pushl %0; popfl" : : "r"(flags));

    *out_pd = proc_pd;
    return ehdr.e_entry;
}
