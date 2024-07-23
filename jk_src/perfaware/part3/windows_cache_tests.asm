global read_asm

section .text

read_asm:
    xor rax, rax
    align 64
.loop:
    vmovdqu ymm0, [rdx + rax]
    vmovdqu ymm0, [rdx + rax + 32]
    add rax, 64
    cmp rax, rcx
    jb .loop
    ret
