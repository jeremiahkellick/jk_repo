global read_asm_custom

section .text

; arg1 (rcx): Outer loop iterations
; arg2 (rdx): Inner loop iterations
; arg3  (r8): Buffer data pointer
read_asm_custom:
    align 64
.outer_loop:
    mov rax, rdx
    mov r9, r8

    .inner_loop:
        vmovdqu ymm0, [r9]
        vmovdqu ymm0, [r9 + 0x20]
        vmovdqu ymm0, [r9 + 0x40]
        vmovdqu ymm0, [r9 + 0x60]
        vmovdqu ymm0, [r9 + 0x80]
        vmovdqu ymm0, [r9 + 0xa0]
        vmovdqu ymm0, [r9 + 0xe0]
        vmovdqu ymm0, [r9 + 0xc0]
        add r9, 0x100
        dec rax
        jnz .inner_loop

    dec rcx
    jnz .outer_loop
    ret
