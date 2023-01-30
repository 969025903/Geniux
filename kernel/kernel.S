[bits 32]
%define ERROR_CODE nop
%define ZERO push 0

extern put_str;
extern idt_table;

section .data

global intr_entry_table
intr_entry_table:
%macro VECTOR 2
section .text
intr%1entry:	;intr[0~32] entry
	
	%2			;ѹ�������
	push ds
	push es
	push fs
	push gs
	pushad		;ѹ��32λ�Ĵ���,EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI,EAX

	;��Ƭ�жϣ���������Ƭ����EOI����Ҫ����Ƭ����EOI
	mov al,0x20
	out 0xa0,al
	out 0x20,al

	push %1		;ѹ���жϺ�

	call [idt_table+%1*4]
	jmp intr_exit;
	
section .data
	dd intr%1entry	;section�����ԣ��Ὣ��ͬ��section������һ�������ͻ��γ�һ�� intr����

%endmacro

section .text
global intr_exit
intr_exit:
	add esp,4	;�����жϺ�
	popad
	pop gs
	pop fs
	pop es
	pop ds
	add esp,4	;����������
	iretd


VECTOR 0x00,ZERO
VECTOR 0x01,ZERO
VECTOR 0x02,ZERO
VECTOR 0x03,ZERO
VECTOR 0x04,ZERO
VECTOR 0x05,ZERO
VECTOR 0x06,ZERO
VECTOR 0x07,ZERO
VECTOR 0x08,ZERO
VECTOR 0x09,ZERO
VECTOR 0x0a,ZERO
VECTOR 0x0b,ZERO
VECTOR 0x0c,ZERO
VECTOR 0x0d,ZERO
VECTOR 0x0e,ZERO
VECTOR 0x0f,ZERO
VECTOR 0x10,ZERO
VECTOR 0x11,ZERO
VECTOR 0x12,ZERO
VECTOR 0x13,ZERO
VECTOR 0x14,ZERO
VECTOR 0x15,ZERO
VECTOR 0x16,ZERO
VECTOR 0x17,ZERO
VECTOR 0x18,ZERO
VECTOR 0x19,ZERO
VECTOR 0x1a,ZERO
VECTOR 0x1b,ZERO
VECTOR 0x1c,ZERO
VECTOR 0x1d,ZERO
VECTOR 0x1e,ERROR_CODE
VECTOR 0x1f,ZERO
VECTOR 0x20,ZERO