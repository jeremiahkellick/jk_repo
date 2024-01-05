bits 16

add bx, [bx+si]
add bx, [bp]
sub bx, [bx+si]
sub bx, [bp]
cmp bx, [bx+si]
cmp bx, [bp]

add byte [bx], 34
add word [bp + si + 1000], 29
sub byte [bx], 34
sub word [bp + si + 1000], 29
cmp byte [bx], 34
cmp word [bp + si + 1000], 29
