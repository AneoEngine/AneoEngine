bits 16
global start

start:

	mov si,msg

.print:
	lodsb
	cmp al,0
	je .continue
	mov ah,0x0E
	int 0x10
	jmp .print

.continue:

	cli

	in al,0x92
	or al,2
	out 0x92,al

	lgdt [gdt_descriptor]

	mov eax,cr0
	or eax,1
	mov cr0,eax

	jmp 0x08:protected_mode


bits 32
protected_mode:

	mov ax,0x10
	mov ds,ax
	mov es,ax
	mov fs,ax
	mov gs,ax
	mov ss,ax
	mov esp,0x90000

	extern kmain
	call kmain

hang:
	jmp hang


msg db "Kernel entry   OK",0


gdt_start:
dq 0
dq 0x00CF9A000000FFFF
dq 0x00CF92000000FFFF
gdt_end:

gdt_descriptor:
dw gdt_end - gdt_start - 1
dd gdt_start
