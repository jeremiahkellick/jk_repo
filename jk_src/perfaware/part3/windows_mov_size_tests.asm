global read_4x2
global read_8x2
global read_16x2
global read_32x2

section .text

read_4x2:
    xor rax, rax
    align 64
.loop:
    mov r8d, [rdx]
    mov r8d, [rdx + 4]
    add rax, 8
    cmp rax, rcx
    jb .loop
    ret

read_8x2:
    xor rax, rax
    align 64
.loop:
    mov r8, [rdx]
    mov r8, [rdx + 8]
    add rax, 16
    cmp rax, rcx
    jb .loop
    ret

read_16x2:
    xor rax, rax
    align 64
.loop:
    vmovdqu xmm0, [rdx]
    vmovdqu xmm1, [rdx + 16]
    add rax, 32
    cmp rax, rcx
    jb .loop
    ret

read_32x2:
    xor rax, rax
    align 64
.loop:
    vmovdqu ymm0, [rdx]
    vmovdqu ymm1, [rdx + 32]
    add rax, 64
    cmp rax, rcx
    jb .loop
    ret
