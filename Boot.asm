[org	0x7C00]
bits	16

KERNEL_OFFSET	equ	0x1000
KERNEL_SECTORS	equ	20

start:

	cli
	xor	ax,ax
	mov	ds,ax
	mov	es,ax
	mov	ss,ax
	mov	sp,0x7C00
	sti

	mov	[BOOT_DRIVE],dl

	mov	ax,0x0003
	int	0x10

	mov	ax,0x1112
	mov	bl,0
	int	0x10

	mov	si,title
	call	Print
	call	NewLine
	call	NewLine

	mov	si,bootmsg
	call	Print
	call	NewLine

	mov	ah,0x00
	mov	dl,[BOOT_DRIVE]
	int	0x13

	mov	si,kernmsg
	call	Print
	call	NewLine

	mov	bx,KERNEL_OFFSET
	mov	si,KERNEL_SECTORS
	mov	cl,2

load_loop:

	push	cx
	push	si

retry:

	mov	ah,0x02
	mov	al,1
	mov	ch,0
	mov	dh,0
	mov	dl,[BOOT_DRIVE]

	int	0x13
	jc	retry

	pop	si
	pop	cx

	add	bx,512
	inc	cl
	dec	si
	jnz	load_loop

	jmp	0x0000:KERNEL_OFFSET


Print:
	lodsb
	cmp	al,0
	je	.done

	mov	ah,0x09
	mov	bh,0
	mov	bl,0x0F
	mov	cx,1
	int	0x10

	mov	ah,0x0E
	int	0x10

	jmp	Print
.done:
	ret


NewLine:
	mov	ah,0x0E
	mov	al,13
	int	0x10
	mov	al,10
	int	0x10
	ret


title		db	"AneoEngine V0.2",0
bootmsg		db	"Bootloader     OK  OCU  0x00007C00-0x00007CFF",0
kernmsg		db	"Kernel sectors DETECTED",0

BOOT_DRIVE	db	0

times	510-($-$$)	db	0
dw	0xAA55
