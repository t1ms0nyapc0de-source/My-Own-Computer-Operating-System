#ifndef ELF_H
#define ELF_H

#include <stdint.h>

uint32_t elf_load(const char* path, uint32_t** out_pd);

#endif
