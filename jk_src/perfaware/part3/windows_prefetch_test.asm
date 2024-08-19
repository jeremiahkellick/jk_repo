global process_nodes_control
global process_nodes_prefetch

section .text

; rcx: repetition count
; rdx: linked list pointer
process_nodes_control:
    align 64

.outer_loop:
    vmovdqa ymm0, [rdx]
    vmovdqa ymm1, [rdx + 0x20]

    mov rdx, [rdx]

    mov r8, rcx
    .inner_loop:
        vpxor ymm0, ymm1
        vpaddd ymm0, ymm1
        dec r8
        jnz .inner_loop

    test rdx, rdx
    jnz .outer_loop

    ret

; rcx: repetition count
; rdx: linked list pointer
process_nodes_prefetch:
    align 64

.outer_loop:
    vmovdqa ymm0, [rdx]
    vmovdqa ymm1, [rdx + 0x20]

    mov rdx, [rdx]
    prefetcht0 [rdx]

    mov r8, rcx
    .inner_loop:
        vpxor ymm0, ymm1
        vpaddd ymm0, ymm1
        dec r8
        jnz .inner_loop

    test rdx, rdx
    jnz .outer_loop

    ret
