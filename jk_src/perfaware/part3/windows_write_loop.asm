global buffer_loop_mov_asm
global buffer_loop_nop_asm
global buffer_loop_cmp_asm
global buffer_loop_dec_asm

section .text

buffer_loop_mov_asm:
    xor rax, rax
.loop:
    mov [rdx + rax], al
    inc rax
    cmp rax, rcx
    jb .loop
    ret

buffer_loop_nop_asm:
    xor rax, rax
.loop:
    db 0x0f, 0x1f, 0x00
    inc rax
    cmp rax, rcx
    jb .loop
    ret

buffer_loop_cmp_asm:
    xor rax, rax
.loop:
    inc rax
    cmp rax, rcx
    jb .loop
    ret

buffer_loop_dec_asm:
.loop:
    dec rcx
    jnz .loop
    ret
