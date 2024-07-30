global read_asm

section .text

; arg1 (rcx): Number of bytes to read (must be a power of 2)
; arg2 (rdx): Buffer size (must be a power of 2)
; arg3  (r8): Buffer data pointer
read_asm:
    xor rax, rax
    mov r9, r8
    sub rdx, 1 ; Convert buffer size to a mask
    align 64
.loop:
    vmovdqu ymm0, [r9]
    vmovdqu ymm0, [r9 + 0x20]
    vmovdqu ymm0, [r9 + 0x40]
    vmovdqu ymm0, [r9 + 0x60]
    vmovdqu ymm0, [r9 + 0x80]
    vmovdqu ymm0, [r9 + 0xa0]
    vmovdqu ymm0, [r9 + 0xe0]
    vmovdqu ymm0, [r9 + 0xc0]

    add rax, 0x100
    mov r9, rax
    and r9, rdx
    add r9, r8

    cmp rax, rcx
    jb .loop
    ret
