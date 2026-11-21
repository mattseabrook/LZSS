; Public domain. Okumura LZSS codec for x86-64.
; System V ABI on ELF/Mach-O and Microsoft x64 ABI on PE/COFF.
default rel

%ifidn __OUTPUT_FORMAT__, macho64
    %define C(name) _ %+ name
%else
    %define C(name) name
%endif
%ifidn __OUTPUT_FORMAT__, win64
    %define arg1 rcx
    %define arg2 rdx
    %define arg3 r8
    %define arg4 r9
%else
    %define arg1 rdi
    %define arg2 rsi
    %define arg3 rdx
    %define arg4 rcx
%endif

extern C(lzss_cli_reserve)
extern C(lzss_cli_progress)
global C(lzss_asm_encode)
global C(lzss_asm_decode)

%define N 4096
%define F 18
%define START (N - F)
%define RING_MASK (N - 1)

; Layout of lzss_tree and lzss_buffer, checked at compile time in C/lzss.c.
%define left 4114
%define right 12308
%define parent 21014
%define match_pos 29208
%define match_len 29212
%define buf_data 0
%define buf_size 8

; 32 bytes of Win64 home space followed by codec locals.
%define frame_size 88
%define last_report 32
%define flag_at 40
%define window_head 48
%define window_tail 52
%define lookahead 56
%define flag_mask 60
%define run 64

%macro enter_codec 0
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, frame_size
.prologue_end:
%endmacro

%macro leave_codec 0
    add rsp, frame_size
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
%endmacro

%macro call_c 1
%ifidn __OUTPUT_FORMAT__, elf64
    call C(%1) wrt ..plt
%else
    call C(%1)
%endif
%endmacro

; r10 = tree, ecx = node. Return the parent in eax and its link in r11.
%macro find_parent_link 0
    movzx eax, word [r10 + parent + rcx*2]
    lea r11, [r10 + right + rax*2]
    cmp [r11], cx
    je %%found
    lea r11, [r10 + left + rax*2]
%%found:
%endmacro

section .text

; r10 = tree, edx = node. Traverse the first-byte bucket and replace a
; full match in place. Strictly longer matches win, as in the C encoder.
tree_insert:
    lea rdi, [r10 + rdx]
    movzx ecx, byte [rdi]
    add ecx, N + 1
    mov word [r10 + left + rdx*2], N
    mov word [r10 + right + rdx*2], N
    mov dword [r10 + match_len], 0
.right:
    lea r11, [r10 + right + rcx*2]
    jmp .visit
.left:
    lea r11, [r10 + left + rcx*2]
.visit:
    movzx eax, word [r11]
    cmp eax, N
    je .attach
    mov ecx, eax
    lea rsi, [r10 + rcx]
    mov r8d, 1
.compare:
    movzx eax, byte [rdi + r8]
    movzx r9d, byte [rsi + r8]
    sub eax, r9d
    jne .consider
    inc r8d
    cmp r8d, F
    jb .compare
.consider:
    cmp r8d, [r10 + match_len]
    jbe .branch
    mov [r10 + match_pos], ecx
    mov [r10 + match_len], r8d
    cmp r8d, F
    je .replace
.branch:
    test eax, eax
    jns .right
    jmp .left
.attach:
    mov [r11], dx
    mov [r10 + parent + rdx*2], cx
    ret
.replace:
    find_parent_link
    mov [r11], dx
    mov [r10 + parent + rdx*2], ax
    movzx eax, word [r10 + left + rcx*2]
    mov [r10 + left + rdx*2], ax
    mov [r10 + parent + rax*2], dx
    movzx eax, word [r10 + right + rcx*2]
    mov [r10 + right + rdx*2], ax
    mov [r10 + parent + rax*2], dx
    mov word [r10 + parent + rcx*2], N
    ret

; r10 = tree, ecx = node. Use the rightmost node of its left subtree
; when both children exist.
tree_erase:
    cmp word [r10 + parent + rcx*2], N
    je .done
    movzx r8d, word [r10 + left + rcx*2]
    movzx r9d, word [r10 + right + rcx*2]
    cmp r8d, N
    jne .has_left
    mov r8d, r9d
    jmp .link
.has_left:
    cmp r9d, N
    je .link
    mov r11d, r8d
.walk:
    movzx eax, word [r10 + right + r8*2]
    cmp eax, N
    je .predecessor
    mov r8d, eax
    jmp .walk
.predecessor:
    cmp r8d, r11d
    je .right_child
    movzx eax, word [r10 + parent + r8*2]
    movzx edx, word [r10 + left + r8*2]
    mov [r10 + right + rax*2], dx
    mov [r10 + parent + rdx*2], ax
    mov [r10 + left + r8*2], r11w
    mov [r10 + parent + r11*2], r8w
.right_child:
    mov [r10 + right + r8*2], r9w
    mov [r10 + parent + r9*2], r8w
.link:
    find_parent_link
    mov [r11], r8w
    mov [r10 + parent + r8*2], ax
    mov word [r10 + parent + rcx*2], N
.done:
    ret

; Keep input, length, output and cursor in nonvolatile registers across
; allocator and progress calls. The C front end supplies encoder scratch.
C(lzss_asm_encode):
    enter_codec
    mov r12, arg1
    mov r13, arg2
    mov r14, arg3
    mov r15, arg4
    test r13, r13
    jz .success

    mov rdi, r15
    mov eax, ' '
    mov ecx, START
    rep stosb
    xor eax, eax
    mov ecx, 2*F - 1
    rep stosb
    lea rdi, [r15 + parent]
    mov eax, N
    mov ecx, N
    rep stosw
    lea rdi, [r15 + right + 2*(N + 1)]
    mov ecx, 256
    rep stosw

    mov ebx, F
    cmp r13, rbx
    cmovb rbx, r13
    mov [rsp + lookahead], ebx
    lea rdi, [r15 + START]
    mov rsi, r12
    mov ecx, ebx
    rep movsb
    ; Match C's seed order. Visiting the same nodes in reverse changes ties.
    mov ebp, 1
.seed:
    mov edx, START
    sub edx, ebp
    mov r10, r15
    call tree_insert
    inc ebp
    cmp ebp, F
    jbe .seed
    mov edx, START
    mov r10, r15
    call tree_insert

    mov dword [rsp + window_head], 0
    mov dword [rsp + window_tail], START
    mov dword [rsp + flag_mask], 1
    mov qword [rsp + last_report], 0
.token:
    mov rax, [rsp + last_report]
    test rax, rax
    jz .progress
    mov rcx, rbx
    sub rcx, rax
    cmp rcx, 1 << 20
    jb .group
.progress:
    mov arg1, rbx
    mov arg2, r13
    call_c lzss_cli_progress
    mov [rsp + last_report], rbx
.group:
    cmp dword [rsp + flag_mask], 1
    jne .match
    mov arg1, r14
    mov arg2, 17
    call_c lzss_cli_reserve
    test al, al
    jz .return
    mov rax, [r14 + buf_size]
    mov rcx, [r14 + buf_data]
    mov [rsp + flag_at], rax
    mov byte [rcx + rax], 0
    inc qword [r14 + buf_size]
.match:
    mov eax, [r15 + match_len]
    cmp eax, [rsp + lookahead]
    cmova eax, [rsp + lookahead]
    mov r8, [r14 + buf_data]
    mov r9, [r14 + buf_size]
    cmp eax, 2
    ja .reference
    mov eax, 1
    mov rdx, [rsp + flag_at]
    mov ecx, [rsp + flag_mask]
    or [r8 + rdx], cl
    mov edx, [rsp + window_tail]
    mov cl, [r15 + rdx]
    mov [r8 + r9], cl
    inc r9
    jmp .advance
.reference:
    mov edx, [r15 + match_pos]
    mov [r8 + r9], dl
    shr edx, 4
    and edx, 0xf0
    lea ecx, [rax - 3]
    or edx, ecx
    mov [r8 + r9 + 1], dl
    add r9, 2
.advance:
    mov [r14 + buf_size], r9
    mov [rsp + run], eax
    shl byte [rsp + flag_mask], 1
    jnz .slide
    mov byte [rsp + flag_mask], 1
.slide:
    mov r10, r15
    mov ecx, [rsp + window_head]
    call tree_erase
    mov edx, [rsp + window_head]
    cmp rbx, r13
    jae .exhausted
    mov al, [r12 + rbx]
    inc rbx
    mov [r15 + rdx], al
    cmp edx, F - 1
    jae .step
    mov [r15 + rdx + N], al
    jmp .step
.exhausted:
    dec dword [rsp + lookahead]
.step:
    inc edx
    and edx, RING_MASK
    mov [rsp + window_head], edx
    mov edx, [rsp + window_tail]
    inc edx
    and edx, RING_MASK
    mov [rsp + window_tail], edx
    cmp dword [rsp + lookahead], 0
    je .success
    mov r10, r15
    call tree_insert
    dec dword [rsp + run]
    jnz .slide
    jmp .token
.success:
    mov eax, 1
.return:
    leave_codec
.end:

; The output is also the history. References that reach before byte zero
; read the initial space and zero regions of the ring.
C(lzss_asm_decode):
    enter_codec
    mov r12, arg1
    mov r13, arg2
    mov r14, arg3
    xor ebx, ebx
.group:
    cmp rbx, r13
    jae .success
    movzx ebp, byte [r12 + rbx]
    inc rbx
    or ebp, 0xff00
.token:
    cmp rbx, r13
    jae .success
    test ebp, 1
    jz .reference
    mov arg1, r14
    mov arg2, 1
    call_c lzss_cli_reserve
    test al, al
    jz .return
    mov rax, [r14 + buf_data]
    mov rcx, [r14 + buf_size]
    mov dl, [r12 + rbx]
    inc rbx
    mov [rax + rcx], dl
    inc qword [r14 + buf_size]
    jmp .next
.reference:
    mov rax, r13
    sub rax, rbx
    cmp rax, 2
    jb .success
    movzx eax, byte [r12 + rbx]
    movzx ecx, byte [r12 + rbx + 1]
    add rbx, 2
    mov edx, ecx
    and edx, 0xf
    add edx, 3
    and ecx, 0xf0
    shl ecx, 4
    or eax, ecx
    mov r15d, [r14 + buf_size]
    add r15d, START - 1
    sub r15d, eax
    and r15d, RING_MASK
    inc r15d
    mov [rsp + run], edx
    mov arg1, r14
    mov arg2, rdx
    call_c lzss_cli_reserve
    test al, al
    jz .return

    mov rdi, [r14 + buf_data]
    mov rdx, [r14 + buf_size]
    mov ecx, [rsp + run]
    lea r9, [rdx + rcx]
    mov [r14 + buf_size], r9
    cmp rdx, r15
    jae .copy_start
.prefill:
    mov rax, r15
    sub rax, rdx
    cmp rax, START + 1
    sbb al, al
    and al, ' '
    mov [rdi + rdx], al
    inc rdx
    dec ecx
    jz .next
    cmp rdx, r15
    jb .prefill
.copy_start:
    lea rsi, [rdi + rdx]
    mov rdi, rsi
    sub rsi, r15
.copy:
    mov al, [rsi]
    mov [rdi], al
    inc rsi
    inc rdi
    dec ecx
    jnz .copy
.next:
    shr ebp, 1
    test ebp, 0x100
    jnz .token
    jmp .group
.success:
    mov eax, 1
.return:
    leave_codec
.end:

%ifidn __OUTPUT_FORMAT__, win64
; Both public routines have the same prologue and Win64 unwind recipe.
section .xdata rdata align=4
unwind_codec:
    db 1, C(lzss_asm_encode).prologue_end - C(lzss_asm_encode), 9, 0
    db 16, ((frame_size - 8)/8 << 4) | 2
    db 12, 15 << 4
    db 10, 14 << 4
    db 8, 13 << 4
    db 6, 12 << 4
    db 4, 7 << 4
    db 3, 6 << 4
    db 2, 5 << 4
    db 1, 3 << 4
    dw 0
section .pdata rdata align=4
    dd C(lzss_asm_encode) wrt ..imagebase, C(lzss_asm_encode).end wrt ..imagebase, unwind_codec wrt ..imagebase
    dd C(lzss_asm_decode) wrt ..imagebase, C(lzss_asm_decode).end wrt ..imagebase, unwind_codec wrt ..imagebase
%elifidn __OUTPUT_FORMAT__, elf64
section .note.GNU-stack noalloc noexec nowrite progbits
%endif
