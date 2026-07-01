global switch_task
extern tss_set_kernel_stack

section .text

switch_task:
    ; Save callee-saved registers of the previous task
    push ebp
    push ebx
    push esi
    push edi

    ; Get pointer to prev task (offset: 4 pushed registers (16 bytes) + 4 bytes return EIP = 20)
    mov eax, [esp + 20]
    ; Save current ESP to prev->esp (offset 4 in task_t)
    mov [eax + 4], esp

    ; Get pointer to next task (offset 24)
    mov ebx, [esp + 24]
    ; Load next task's ESP from next->esp
    mov esp, [ebx + 4]

    ; Check if Page Directory (CR3) needs to change
    mov ecx, [ebx + 8]   ; next->cr3
    mov edx, cr3
    cmp ecx, edx
    je .no_pd_switch
    mov cr3, ecx
.no_pd_switch:

    ; Update TSS esp0 with the next task's kernel stack top
    mov ecx, [ebx + 12]  ; next->kstack
    push ecx
    call tss_set_kernel_stack
    add esp, 4

    ; Restore callee-saved registers of the new task
    pop edi
    pop esi
    pop ebx
    pop ebp

    ret
