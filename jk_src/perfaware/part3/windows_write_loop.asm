global buffer_loop_mov
global buffer_loop_nop
global buffer_loop_cmp
global buffer_loop_dec

section .text

buffer_loop_mov:
    xor rax, rax
.loop:
    mov [rdx + rax], al
    inc rax
    cmp rax, rcx
    jb .loop
    ret

buffer_loop_nop:
    xor rax, rax
.loop:
    db 0x0f, 0x1f, 0x00
    inc rax
    cmp rax, rcx
    jb .loop
    ret

buffer_loop_cmp:
    xor rax, rax
.loop:
    inc rax
    cmp rax, rcx
    jb .loop
    ret

buffer_loop_dec:
.loop:
    dec rcx
    jnz .loop
    ret
