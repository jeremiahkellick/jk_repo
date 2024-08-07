global test_control
global test_non_temporal

section .text

; arg1 (rcx): Outer loop iterations
; arg2 (rdx): Inner loop iterations
; arg4  (r8): Input buffer data pointer
; arg4  (r9): Output buffer data pointer
test_control:
    align 64
.outer_loop:
    vmovdqu ymm0, [r8]
    vmovdqu ymm1, [r8 + 0x20]
    vmovdqu ymm2, [r8 + 0x40]
    vmovdqu ymm3, [r8 + 0x60]

    mov rax, rdx ; Copy inner_loop_iterations to counter
    .inner_loop:
        vmovdqu [r9], ymm0
        vmovdqu [r9 + 0x20], ymm1
        vmovdqu [r9 + 0x40], ymm2
        vmovdqu [r9 + 0x60], ymm3
        add r9, 0x80
        dec rax
        jnz .inner_loop

    add r8, 0x80
    dec rcx
    jnz .outer_loop
    ret

; arg1 (rcx): Outer loop iterations
; arg2 (rdx): Inner loop iterations
; arg4  (r8): Input buffer data pointer
; arg4  (r9): Output buffer data pointer
test_non_temporal:
    align 64
.outer_loop:
    vmovdqu ymm0, [r8]
    vmovdqu ymm1, [r8 + 0x20]
    vmovdqu ymm2, [r8 + 0x40]
    vmovdqu ymm3, [r8 + 0x60]

    mov rax, rdx ; Copy inner_loop_iterations to counter
    .inner_loop:
        vmovntdq [r9], ymm0
        vmovntdq [r9 + 0x20], ymm1
        vmovntdq [r9 + 0x40], ymm2
        vmovntdq [r9 + 0x60], ymm3
        add r9, 0x80
        dec rax
        jnz .inner_loop

    add r8, 0x80
    dec rcx
    jnz .outer_loop
    ret
