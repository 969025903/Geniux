#include "interrupt.h"
#include "stdint.h"
#include "global.h"
#include "io.h"
#include "print.h"

#define PIC_M_CTRL 0x20	       // �����õĿɱ���жϿ�������8259A,��Ƭ�Ŀ��ƶ˿���0x20
#define PIC_M_DATA 0x21	       // ��Ƭ�����ݶ˿���0x21
#define PIC_S_CTRL 0xa0	       // ��Ƭ�Ŀ��ƶ˿���0xa0
#define PIC_S_DATA 0xa1	       // ��Ƭ�����ݶ˿���0xa1

#define IDT_DESC_CNT 0x81	       // Ŀǰ�ܹ�֧�ֵ��ж���

#define EFLAGS_IF   0x00000200        // eflags�Ĵ����е�ifλΪ1
#define GET_EFLAGS(EFLAG_VAR) asm volatile("pushfl; popl %0" : "=g" (EFLAG_VAR))



/*�ж����������ṹ��*/
struct gate_desc {
	uint16_t    func_offset_low_word;
	uint16_t    selector;
	uint8_t     dcount;   //����Ϊ˫�ּ����ֶΣ������������еĵ�4�ֽڡ�����̶�ֵ�����ÿ���
	uint8_t     attribute;
	uint16_t    func_offset_high_word;
};

// ��̬��������,�Ǳ���
static void pic_init(void);
static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function);
static void general_intr_handler(uint8_t vec_nr);
static void exception_init(void);
static struct gate_desc idt[IDT_DESC_CNT];   	     // idt���ж���������,�����Ͼ��Ǹ��ж�������������
char* intr_name[IDT_DESC_CNT];		     // ���ڱ����쳣������   
extern uint32_t syscall_handler(void);

/********    �����жϴ����������    ********
 * ��kernel.S�ж����intrXXentryֻ���жϴ����������,
 * ���յ��õ���ide_table�еĴ������*/
intr_handler idt_table[IDT_DESC_CNT];

/********************************************/
extern intr_handler intr_entry_table[IDT_DESC_CNT];	    // �������ö�����kernel.S�е��жϴ������������





/* ��ʼ���ɱ���жϿ�����8259A */
static void pic_init(void) {

	/* ��ʼ����Ƭ */
	outb(PIC_M_CTRL, 0x11);   // ICW1: ���ش���,����8259, ��ҪICW4.
	outb(PIC_M_DATA, 0x20);   // ICW2: ��ʼ�ж�������Ϊ0x20,Ҳ����IR[0-7] Ϊ 0x20 ~ 0x27.
	outb(PIC_M_DATA, 0x04);   // ICW3: IR2�Ӵ�Ƭ. 
	outb(PIC_M_DATA, 0x01);   // ICW4: 8086ģʽ, ����EOI

	/* ��ʼ����Ƭ */
	outb(PIC_S_CTRL, 0x11);    // ICW1: ���ش���,����8259, ��ҪICW4.
	outb(PIC_S_DATA, 0x28);    // ICW2: ��ʼ�ж�������Ϊ0x28,Ҳ����IR[8-15] Ϊ 0x28 ~ 0x2F.
	outb(PIC_S_DATA, 0x02);    // ICW3: ���ô�Ƭ���ӵ���Ƭ��IR2����
	outb(PIC_S_DATA, 0x01);    // ICW4: 8086ģʽ, ����EOI

	outb(PIC_M_DATA, 0xf8);
	outb(PIC_S_DATA, 0xbf);

	put_str("   pic_init done\n");
}

/* �����ж��������� */
static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function) {
	p_gdesc->func_offset_low_word = (uint32_t)function & 0x0000FFFF;
	p_gdesc->selector = SELECTOR_K_CODE;
	p_gdesc->dcount = 0;
	p_gdesc->attribute = attr;
	p_gdesc->func_offset_high_word = ((uint32_t)function & 0xFFFF0000) >> 16;
}

/*��ʼ���ж���������*/
static void idt_desc_init(void) {
	int i, lastindex = IDT_DESC_CNT - 1;
	for (i = 0; i < IDT_DESC_CNT; i++) {
		make_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]);
	}

	make_idt_desc(&idt[lastindex],IDT_DESC_ATTR_DPL3,syscall_handler);
   /* ��������ϵͳ����,ϵͳ���ö�Ӧ���ж���dplΪ3,
	* �жϴ������Ϊ������syscall_handler */
	put_str("   idt_desc_init done\n");
}



/* ͨ�õ��жϴ�����,һ�������쳣����ʱ�Ĵ��� */
static void general_intr_handler(uint8_t vec_nr) {
	if (vec_nr == 0x27 || vec_nr == 0x2f) {	// 0x2f�Ǵ�Ƭ8259A�ϵ����һ��irq���ţ�����
		return;		//IRQ7��IRQ15�����α�ж�(spurious interrupt),���봦��
	}
	set_cursor(0);                                         //���������0��λ
	int cursor_pos = 0;
	while ((cursor_pos++) < 320)			    //һ��80�� 4�пո�
		put_char(' ');

	set_cursor(0);
	put_str("!!!!!!            excetion message begin            !!!!!!\n");
	set_cursor(88);					    //�ڶ��еڰ˸��ֿ�ʼ��ӡ
	put_str(intr_name[vec_nr]);                            //��ӡ�ж�������
	if (vec_nr == 14)
	{
		int page_fault_vaddr = 0;
		asm("movl %%cr2,%0" : "=r" (page_fault_vaddr));   //�������ַ ����ķŵ��������������
		put_str("\npage fault addr is ");
		put_int(page_fault_vaddr);
	}
	put_str("!!!!!!            excetion message end              !!!!!!\n");

	while (1);                                              //��ͣ
}


/* ���һ���жϴ�����ע�ἰ�쳣����ע�� */
static void exception_init(void) {			    // ���һ���жϴ�����ע�ἰ�쳣����ע��
	int i;
	for (i = 0; i < IDT_DESC_CNT; i++) {

		/* idt_table�����еĺ������ڽ����жϺ�����ж������ŵ��õ�,
		 * ��kernel/kernel.S��call [idt_table + %1*4] */
		idt_table[i] = general_intr_handler;		    // Ĭ��Ϊgeneral_intr_handler��
								  // �Ժ����register_handler��ע����崦������
		intr_name[i] = "unknown";				    // ��ͳһ��ֵΪunknown 
	}
	intr_name[0] = "#DE Divide Error";
	intr_name[1] = "#DB Debug Exception";
	intr_name[2] = "NMI Interrupt";
	intr_name[3] = "#BP Breakpoint Exception";
	intr_name[4] = "#OF Overflow Exception";
	intr_name[5] = "#BR BOUND Range Exceeded Exception";
	intr_name[6] = "#UD Invalid Opcode Exception";
	intr_name[7] = "#NM Device Not Available Exception";
	intr_name[8] = "#DF Double Fault Exception";
	intr_name[9] = "Coprocessor Segment Overrun";
	intr_name[10] = "#TS Invalid TSS Exception";
	intr_name[11] = "#NP Segment Not Present";
	intr_name[12] = "#SS Stack Fault Exception";
	intr_name[13] = "#GP General Protection Exception";
	intr_name[14] = "#PF Page-Fault Exception";
	// intr_name[15] ��15����intel�����δʹ��
	intr_name[16] = "#MF x87 FPU Floating-Point Error";
	intr_name[17] = "#AC Alignment Check Exception";
	intr_name[18] = "#MC Machine-Check Exception";
	intr_name[19] = "#XF SIMD Floating-Point Exception";
	//intr_name[80] = "SYSCALL INTR";
}


/*����й��жϵ����г�ʼ������*/
void idt_init() {
	put_str("idt_init start\n");
	idt_desc_init();	   // ��ʼ���ж���������
	exception_init();	   // �쳣����ʼ����ע��ͨ�����жϴ�����
	pic_init();		   // ��ʼ��8259A

	/* ����idt */
	uint64_t idt_operand = ((sizeof(idt) - 1) | ((uint64_t)(uint32_t)idt << 16));
	asm volatile("lidt %0" : : "m" (idt_operand));
	put_str("idt_init done\n");
}

void register_handler(uint8_t vec_no, intr_handler function)
{
	//����������ŵ�ע�ắ��ָ��Ž�ȥ��
	idt_table[vec_no] = function;
}

enum intr_status intr_enable()
{
	if (intr_get_status() != INTR_ON)
	{
		asm volatile("sti");
		return INTR_OFF;
	}
	return INTR_ON;
}

enum intr_status intr_disable()
{
	if (intr_get_status() != INTR_OFF)
	{
		asm volatile("cli");
		return INTR_ON;
	}
	return INTR_OFF;
}

enum intr_status intr_set_status(enum intr_status status)
{
	return (status == INTR_ON) ? intr_enable() : intr_disable();
}

enum intr_status intr_get_status()
{
	uint32_t eflags = 0;
	GET_EFLAGS(eflags);
	return (eflags & EFLAGS_IF) ? INTR_ON : INTR_OFF;
}
