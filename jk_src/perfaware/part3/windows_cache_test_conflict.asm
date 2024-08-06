global read_asm_conflict

section .text

; arg1 (rcx): Outer loop iterations
; arg2 (rdx): Inner loop iterations
; arg3  (r8): Buffer data pointer
read_asm_conflict:
    align 64
.outer_loop:
    mov rax, rdx
    mov r9, r8

    .inner_loop:
        vmovdqu ymm0, [r9]
        vmovdqu ymm0, [r9 + 0x20]
        vmovdqu ymm0, [r9 + 0x10000]
        vmovdqu ymm0, [r9 + 0x10020]
        vmovdqu ymm0, [r9 + 0x20000]
        vmovdqu ymm0, [r9 + 0x20020]
        vmovdqu ymm0, [r9 + 0x30000]
        vmovdqu ymm0, [r9 + 0x30020]
        add r9, 0x40000
        dec rax
        jnz .inner_loop

    dec rcx
    jnz .outer_loop
    ret
