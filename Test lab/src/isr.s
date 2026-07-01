extern isr_dispatch
extern irq_dispatch

global idt_load

idt_load:
    mov eax, [esp+4]
    lidt [eax]
    ret

; Macro for exceptions that don't push error codes
%macro ISR_NOERR 1
global isr%1
isr%1:
    push byte 0      ; Push dummy error code
    push %1          ; Push exception number
    jmp isr_common_stub
%endmacro

; Macro for exceptions that push error codes
%macro ISR_ERR 1
global isr%1
isr%1:
    push %1          ; Push exception number
    jmp isr_common_stub
%endmacro

; Macro for IRQs
%macro IRQ 2
global irq%1
irq%1:
    push byte 0      ; Push dummy error code
    push %2          ; Push mapped interrupt number
    jmp irq_common_stub
%endmacro

; Define exception stubs
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31

; Syscall stub (vector 128 / 0x80)
global isr128
isr128:
    push byte 0      ; Dummy error code
    push 128         ; Syscall interrupt number (128)
    jmp isr_common_stub

; Define IRQ stubs
IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

isr_common_stub:
    pusha            ; Pushes edi, esi, ebp, esp, ebx, edx, ecx, eax

    mov ax, ds
    push eax         ; Save data segment

    mov ax, 0x10     ; Load kernel data segment descriptor
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp         ; Pass register state structure to dispatcher
    call isr_dispatch
    add esp, 4

    pop eax          ; Restore original data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa             ; Restore general-purpose registers
    add esp, 8       ; Clean up error code and interrupt number
    iret

irq_common_stub:
    pusha

    mov ax, ds
    push eax

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call irq_dispatch
    add esp, 4

    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8
    iret
