[bits 32]
section .text
global switch_to
switch_to:

	;�����ں�ջ
	push esi
	push edi
	push ebx
	push ebp

	;�õ�ջ�в���
	mov eax,[esp+20]	;��ȡcur����cur��espָ�뱣����self_kstack��
	mov [eax],esp

	;�ϰ벿���Ǳ���cur�̵߳�ջ���ݣ��°벿���ǻָ�next��ջ����
	mov eax,[esp+24]	;�õ�next����
	mov esp,[eax]		;pcb�ĵ�һ����Աself_kstack

	pop ebp
	pop ebx
	pop edi
	pop esi
	ret		;  ��ʱ��retִ�е���ջ����kernel_thread