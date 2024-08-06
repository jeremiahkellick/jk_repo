global test_control
global test_non_temporal

section .text

; arg1 (rcx): Input size
; arg2 (rdx): Output size
; arg4  (r8): Buffer data pointer
test_control:
    ; Set output pointer
    mov rax, r8
    add rax, rcx

    ; Set endpoint
    mov r10, rax
    add r10, rdx

    align 64
.outer_loop:
    xor r9, r9 ; Read offset

    .inner_loop:
        vmovdqu ymm0, [r8 + r9]
        vmovdqu ymm1, [r8 + r9 + 0x20]
        vmovdqu [rax], ymm0
        vmovdqu [rax + 0x20], ymm1
        add rax, 0x40
        add r9, 0x40
        cmp r9, rcx
        jb .inner_loop

    cmp rax, r10
    jb .outer_loop
    ret

; arg1 (rcx): Input size
; arg2 (rdx): Output size
; arg4  (r8): Buffer data pointer
test_non_temporal:
    ; Set output pointer
    mov rax, r8
    add rax, rcx

    ; Set endpoint
    mov r10, rax
    add r10, rdx

    align 64
.outer_loop:
    xor r9, r9 ; Read offset

    .inner_loop:
        vmovdqu ymm0, [r8 + r9]
        vmovdqu ymm1, [r8 + r9 + 0x20]
        vmovntdq [rax], ymm0
        vmovntdq [rax + 0x20], ymm1
        add rax, 0x40
        add r9, 0x40
        cmp r9, rcx
        jb .inner_loop

    cmp rax, r10
    jb .outer_loop
    ret
