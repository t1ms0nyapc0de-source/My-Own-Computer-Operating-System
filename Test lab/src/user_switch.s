global enter_user_mode

section .text

enter_user_mode:
    ; entry point is at [esp + 4]
    ; user esp is at [esp + 8]
    
    mov ecx, [esp + 4]   ; target EIP
    mov edx, [esp + 8]   ; target ESP

    ; Load user data segment selectors (0x20 | 3 = 0x23)
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Build the iret stack frame
    push 0x23            ; User SS
    push edx             ; User ESP
    
    ; Push EFLAGS with Interrupt Flag (IF) enabled
    pushfd
    pop eax
    or eax, 0x200        ; Set bit 9 (Interrupt Flag)
    push eax

    push 0x1B            ; User CS (0x18 | 3 = 0x1B)
    push ecx             ; User EIP

    cli
    iret
