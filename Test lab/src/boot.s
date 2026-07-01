MBOOT_PAGE_ALIGN    equ 1 << 0
MBOOT_MEM_INFO      equ 1 << 1
MBOOT_HEADER_MAGIC  equ 0x1BADB002
MBOOT_HEADER_FLAGS  equ MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO
MBOOT_CHECKSUM      equ -(MBOOT_HEADER_MAGIC + MBOOT_HEADER_FLAGS)

section .multiboot
align 4
    dd MBOOT_HEADER_MAGIC
    dd MBOOT_HEADER_FLAGS
    dd MBOOT_CHECKSUM

section .text
global _start
extern kernel_main

_start:
    ; Set up the stack
    mov esp, stack_top

    ; Push multiboot structure pointer (EBX) and magic (EAX)
    push ebx
    push eax

    ; Call the kernel entry point
    call kernel_main

    ; If kernel returns, halt the CPU
.loop:
    cli
    hlt
    jmp .loop

section .bss
align 16
stack_bottom:
    resb 16384 ; 16 KB stack
stack_top:
