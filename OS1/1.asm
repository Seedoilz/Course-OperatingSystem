section   .data
err db "error!!! \n", 0h
linesep db 0ah,0dh

section .bss
    output: resb 50
    spac: resb 1 
    input: resb 50
    space: resb 1
    head: resb 21
    spacee: resb 1
    tail: resb 21 
    spaceee: resb 1  
    calSym: resb 1

section   .text
global _start
    
;function strlen start
strlen:
    push ebx    ;put data in ebx in the stack so that it will not be changed by the fucntion
    mov ebx, eax
 
.nextchar:    cmp byte [eax], 0
    jz .finished
    inc eax
    jmp .nextchar
 
.finished:
    sub eax, ebx
    pop ebx        ;将堆栈中保存的ebx值返回到ebx寄存器中
    ret            ;返回调用函数的地方
;function strlen end

jinwei:
    sub al,10
    mov ecx,1
    ret

puts:
    push edx
    push ecx
    push ebx

    mov ebx,1
    mov ecx,eax
    call strlen
    mov edx, eax
    mov eax, 4
    int 80h

    pop ebx
    pop ecx
    pop edx
    ret

getinput:
    push edx
    push ecx

    mov edx, ebx
    mov ecx, eax
    mov ebx, 0
    mov eax, 3
    int 80h

    pop ecx
    pop edx
    ret

;function processing the input
cutInput:
;process the first num
.cutloop1:
    cmp BYTE[ecx], 42
    jl .errSym
    cmp BYTE[ecx], 57
    jg .errSym
    cmp BYTE[ecx], 48
    jl .check
.back2cut:
    ;find the calculation symbol
    cmp BYTE[ecx], 42
    jz .mulSym
    cmp BYTE[ecx], 43
    jz .addSym

    ;save the num
    mov dl,BYTE[ecx]
    mov byte[eax], dl
    inc eax
    inc ecx
    jmp .cutloop1

.check:
    cmp BYTE[ecx], 43
    jg .errSym
    jmp .back2cut

.errSym:
    push eax
    push ebx
    push ecx
    
    mov eax,err
    mov ebx,1
    mov ecx,eax
    mov edx, 9
    mov eax, 4
    int 80h

    pop eax
    pop ebx
    pop ecx

    call newline
    call _start


;if the cal symbol is add
.addSym:
    mov eax,calSym
    mov dl,BYTE[ecx]
    mov BYTE[eax], dl
    inc ecx
    jmp .cutloop2

;if the cal symbol is mul
.mulSym:
    mov eax,calSym 
    mov dl,BYTE[ecx]
    mov BYTE[eax], dl
    inc ecx
    jmp .cutloop2

;second num process
.cutloop2:
    cmp BYTE[ecx], 10
    jz .rett
    mov dl,BYTE[ecx]
    mov BYTE[ebx],dl
    inc ebx
    inc ecx
    jmp .cutloop2

.rett:
    inc ecx
    ret
;function processing the input ends;
_start:
    mov ebp, esp; for correct debugging
    
    call clear
    mov ebp, esp; for correct debugging
    mov eax,input
    
    mov ebx,50
    call getinput

    mov ecx,input

    cmp byte[ecx],113
    je .ending

    mov eax,head
    ;call cutInput
    mov ebx,tail
    call cutInput

    ;put the address of two strings'ends each in the esi and edi
    mov eax,head
    call strlen
    mov esi,eax
    add esi,head
    sub esi,1

    mov eax, tail
    call strlen
    mov edi, eax
    add edi,tail
    sub edi,1
    
    mov edx,output
    add edx,49
    
    mov ecx,0
    ;add or mul
    cmp BYTE[calSym], 42
    jz .mulLoop
    cmp BYTE[calSym], 43
    jz .addLoop

.ending:
    mov ebx, 0
    mov eax, 1    ;  sys_exit
    int 0x80         ;  系统调用

.output:
   
    mov eax,output
    call findzero
    call puts
    call newline
    call _start


.mulLoop:
    cmp edi, tail
    jl .before_output
    push esi
    push ebx
    push edx

.innerLoop:
    cmp esi, head
    jl .back
    xor eax, eax
    xor ebx, ebx
    add al, byte[esi]
    add bl, byte[edi]
    sub al,48
    sub bl,48
    mul bl
    add byte[edx], al
    mov al, byte[edx]
    mov ah, 0
    xor ecx, ecx
    mov cl,10
    div cl
    mov byte[edx], ah
    dec esi
    dec edx
    add byte[edx], al
    jmp .innerLoop

.back:
    mov eax, edx
    pop edx
    pop ebx
    pop esi
    dec edi
    dec edx
    jmp .mulLoop


.before_output:
    mov edx, eax
    push edx
    push eax
.format:
    ;int to char
    mov eax, output
    mov edx,output
    add edx,49
    call findzero
.format_loop:
    cmp edx,eax
    jl .output_mul
    add byte[edx],48
    dec edx
    jmp .format_loop
    
.output_mul:
    call puts
    call newline
    call _start

    
.addLoop:
    cmp esi, head
    jl .tail_rest
    cmp edi, tail
    jl .head_rest

    xor eax, eax
    add al, BYTE[esi]
    add al, BYTE[edi]
    add al, cl
    sub al, 48  ;ascii to num only once since the result will be char
    dec esi
    dec edi
    dec edx
    mov ecx, 0 ;reset
    cmp al, 57
    mov byte[edx], al
    jle .addLoop
    call jinwei
    mov byte[edx], al
    jmp .addLoop

.head_rest:
    cmp esi, head
    jl .after_add
    xor eax, eax
    add al, byte[esi]
    add al, cl
    dec esi
    dec edx
    mov ecx,0
    mov byte[edx], al
    cmp al, 57
    jle .head_rest
    call jinwei
    mov byte[edx], al
    jmp .head_rest

.tail_rest:
    cmp edi, tail
    jl .after_add
    xor eax, eax
    add al, byte[edi]
    add al, cl
    dec edi
    dec edx
    mov ecx,0
    mov byte[edx], al
    cmp al, 57
    jle .tail_rest
    call jinwei
    mov byte[edx], al
    jmp .tail_rest

.upgradeone:
    dec edx
    mov byte[edx], 49
    jmp .output

.after_add:
    cmp ecx, 1
    jz .upgradeone
    jmp .output

findzero:
    push ebx
    mov ebx,input
.s: 
    cmp ebx,eax
    je .sb
    cmp byte[eax],0
    jne .rettt
    inc eax
    jmp .s
.rettt:
    pop ebx
    ret  
.sb:
    pop ebx
    dec eax
    dec eax
    ret


newline:                     ;显示回车换行
    mov eax,linesep
    call puts
    ret

clear:
    push eax
    push ebx

    mov eax,output
    xor ebx,ebx
.cloop:
    cmp ebx,1000
    je .endings
    mov byte[eax],0
    inc ebx
    inc eax
    jmp .cloop

.endings:
    pop eax
    pop ebx
    ret