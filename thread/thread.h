#ifndef __THREAD_THREAD_H
#define __THREAD_THREAD_H
#include "stdint.h"
#include "list.h"
#include "../kernel/memory.h"

#define PG_SIZE 4096
extern struct list thread_ready_list, thread_all_list;

typedef void thread_func(void*); //�����е㲻�������ʲô��˼ �����Ѳ��� �����Ǻ������� 
typedef int16_t pid_t;

enum task_status
{
	TASK_RUNNING, // 0
	TASK_READY,   // 1
	TASK_BLOCKED, // 2
	TASK_WAITING, // 3
	TASK_HANGING, // 4
	TASK_DIED     // 5
};

/*         intr_stack ���ڴ����жϱ��л��������Ļ������� */
/*	   ��������ȥ����һ�� Ϊʲô�Ƿ��ŵ� Խ�ں���Ĳ��� ��ַԽ�� */

struct intr_stack
{
	uint32_t vec_no; //�жϺ�
	uint32_t edi;
	uint32_t esi;
	uint32_t ebp;
	uint32_t esp_dummy;
	uint32_t ebx;
	uint32_t edx;
	uint32_t ecx;
	uint32_t eax;
	uint32_t gs;
	uint32_t fs;
	uint32_t es;
	uint32_t ds;

	uint32_t err_code;
	void(*eip) (void);        //����������һ������ָ�� 
	uint32_t cs;
	uint32_t eflags;
	void* esp;
	uint32_t ss;
};


/*	�߳�ջ �����̻߳��� */

struct thread_stack
{
	uint32_t ebp;
	uint32_t ebx;
	uint32_t edi;
	uint32_t esi;

	void(*eip) (thread_func* func, void* func_arg); //��������໥��Ӧ ��ret ���������kernel_thread��������

	void(*unused_retaddr);                         //ռλ�� ��ջ��վס�˷��ص�ַ��λ�� ��Ϊ�ǻ��ret 
	thread_func* function;                          //����kernel_threadҪ���õĺ�����ַ
	void* func_arg;				      //����ָ��
};

struct task_struct
{
	uint32_t* self_kstack;                          //pcb�е� kernel_stack �ں�ջ
	pid_t pid;
	enum task_status status;                        //�߳�״̬
	uint8_t priority;				      //��Ȩ��
	uint8_t ticks;				      //��cpu ���еĵδ��� ��ticks ���ж��Ƿ�������ʱ��Ƭ
	uint32_t elapsed_ticks;                         //һ��ִ���˶��
	char name[16];

	struct list_elem general_tag;                   //���������е����ӽڵ�
	struct list_elem all_list_tag;		      //�ܶ��е����ӽڵ�

	uint32_t* pgdir;				      //�����Լ�ҳ��������ַ �߳�û��   
	struct virtual_addr userprog_vaddr;	      //�û����̵�����ռ�
	struct mem_block_desc u_block_desc[DESC_CNT];
	uint32_t stack_magic;			      //Խ����  ��Ϊ����pcb����ľ�������Ҫ�õ�ջ�� ��ʱ��ҪԽ����
};

struct task_struct* running_thread(void);
void kernel_thread(thread_func* function, void* func_arg);
void thread_create(struct task_struct* pthread, thread_func function, void* func_arg);
void init_thread(struct task_struct* pthread, char* name, int prio);
struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg);
void make_main_thread(void);
void schedule(void);
void thread_init(void);
void thread_block(enum task_status stat);
void thread_unblock(struct task_struct* pthread);
void thread_yield(void);
#endif
