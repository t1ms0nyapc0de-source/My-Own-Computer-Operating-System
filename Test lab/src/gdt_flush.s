global gdt_flush

section .text

gdt_flush:
    mov eax, [esp + 4]
    lgdt [eax]

    ; Reload data segments with Kernel Data segment (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Far jump to reload Code Segment (CS) with Kernel Code (0x08)
    jmp 0x08:.flush

.flush:
    ; Load TSS selector (0x28 | 3 = 0x2B)
    mov ax, 0x2B
    ltr ax
    ret
