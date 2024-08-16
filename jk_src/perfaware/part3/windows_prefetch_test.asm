global process_nodes_control
global process_nodes_prefetch

section .text

process_nodes_control:
    push rdi
    mov rdi, 3
    xor rdx, rdx

    align 64
.loop:
    mov eax, [rcx + 8]
    mov rcx, [rcx]
    mov rdx, 0
    div rdi
    test rcx, rcx
    jnz .loop

    pop rdi
    ret

process_nodes_prefetch:
    push rdi
    mov rdi, 3
    xor rdx, rdx

    align 64
.loop:
    mov eax, [rcx + 8]
    mov rcx, [rcx]
    prefetcht0 [rcx]
    mov rdx, 0
    div rdi
    test rcx, rcx
    jnz .loop

    pop rdi
    ret
