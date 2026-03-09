global fma_chain

section .text

fma_chain:
    xor rax, rax
    pxor xmm0, xmm0
    pxor xmm1, xmm1

    align 64
.outer_loop:
    xor r10, r10
    pxor xmm2, xmm2

    .inner_loop:
        vfmadd213sd xmm2, xmm0, xmm1
        vfmadd213sd xmm2, xmm0, xmm1
        vfmadd213sd xmm2, xmm0, xmm1
        vfmadd213sd xmm2, xmm0, xmm1
        vfmadd213sd xmm2, xmm0, xmm1
        vfmadd213sd xmm2, xmm0, xmm1

        add rax, 6
        add r10, 6
        cmp r10, rdx
        jb .inner_loop

    cmp rax, rcx
    jb .outer_loop
    ret
