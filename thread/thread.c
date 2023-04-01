#include "thread.h"   //�������� ���ֽṹ��
#include "stdint.h"   //ǰ׺
#include "string.h"   //memset
#include "global.h"   //�����
#include "memory.h"   //����ҳ��Ҫ
#include "debug.h"
#include "interrupt.h"
#include "print.h"
#include "../userprog/process.h"
#include "../thread/sync.h"

struct task_struct* main_thread;                        //���߳�main_thread��pcb
struct task_struct* idle_thread;						//idle�߳�
struct list thread_ready_list;			  //��������
struct list thread_all_list;				  //���̶߳���
struct lock pid_lock;

extern void switch_to(struct task_struct* cur, struct task_struct* next);


/*ϵͳ����ʱ���е��߳�*/
static void idle(int arg UNUSED) {
	while (1) {
		thread_block(TASK_BLOCKED);
		asm volatile("sti; hlt":::"memory");
	}
}
// ��ȡ pcb ָ��
// �ⲿ���ҿ�������΢����һ��
// �����߳����ڵ�esp �϶����� ����get�õ�����һҳ�ڴ� pcbҳ���¸��� �������ǵ�pcb������ʼλ���������� ��ȥ�����12λ
// ��ô���Ƕ�ǰ���ȡ & ����Եõ� ���ǵĵ�ַ���ڵ�
pid_t allocate_pid(void)
{
	static pid_t next_pid = 0;			  //Լ����ȫ�ֱ��� ȫ����+���޸���
	try_lock(&pid_lock);
	++next_pid;
	try_release(&pid_lock);
	return next_pid;
}

struct task_struct* running_thread(void)
{
	uint32_t esp;
	asm("mov %%esp,%0" : "=g"(esp));
	return (struct task_struct*)(esp & 0xfffff000);
}


void kernel_thread(thread_func* function, void* func_arg)
{
	intr_enable();					    //���ж� ��ֹ�����ʱ���жϱ������޷��л��߳�
	function(func_arg);
}

void thread_create(struct task_struct* pthread, thread_func function, void* func_arg)
{
	pthread->self_kstack -= sizeof(struct intr_stack);  //��ȥ�ж�ջ�Ŀռ�
	pthread->self_kstack -= sizeof(struct thread_stack);
	struct thread_stack* kthread_stack = (struct thread_stack*)pthread->self_kstack;
	kthread_stack->eip = kernel_thread;                 //��ַΪkernel_thread ��kernel_thread ִ��function
	kthread_stack->function = function;
	kthread_stack->func_arg = func_arg;
	kthread_stack->ebp = kthread_stack->ebx = kthread_stack->ebx = kthread_stack->esi = 0; //��ʼ��һ��
	return;
}

void init_thread(struct task_struct* pthread, char* name, int prio)
{
	memset(pthread, 0, sizeof(*pthread)); //pcbλ����0
	strcpy(pthread->name, name);

	if (pthread == main_thread)
		pthread->status = TASK_RUNNING;                              //���ǵ����߳̿϶��������е�
	else
		pthread->status = TASK_READY;					//�ŵ�������������

	pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE); //�տ�ʼ��λ�������λ�� ջ��λ��+һҳ�����
																	 //���滹Ҫ�����ֵ�����޸�
	pthread->pid = allocate_pid();                                   //��ǰ����pid                          
	pthread->priority = prio;
	pthread->ticks = prio;                                           //����Ȩ�� ��ͬ��ʱ��Ƭ
	pthread->elapsed_ticks = 0;
	pthread->pgdir = NULL;                                           //�߳�û�е����ĵ�ַ
	pthread->stack_magic = 0x66666666;                               //���õ�ħ�� ����Ƿ�Խ����
}

struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg)
{
	struct task_struct* thread = get_kernel_pages(1);
	init_thread(thread, name, prio);
	thread_create(thread, function, func_arg);
	ASSERT(!find(&thread_ready_list, &thread->general_tag));     //֮ǰ��Ӧ���ھ�����������
	append(&thread_ready_list, &thread->general_tag);

	ASSERT(!find(&thread_all_list, &thread->all_list_tag));
	append(&thread_all_list, &thread->all_list_tag);

	return thread;
}


//֮ǰ��loader.S��ʱ���Ѿ� mov esp,0xc0009f00
//���ڵ�esp�Ѿ�����Ԥ����pcbλ������
void make_main_thread(void)
{
	main_thread = running_thread();					//�õ�main_thread ��pcbָ��
	init_thread(main_thread, "main", 31);

	ASSERT(!find(&thread_all_list, &main_thread->all_list_tag));
	append(&thread_all_list, &main_thread->all_list_tag);

}

void schedule(void)
{
	ASSERT(intr_get_status() == INTR_OFF);

	//������������޿��������񣬾ͻ���idle
	if (empty(&thread_ready_list)) {
		thread_unblock(idle_thread);
	}
	struct task_struct* cur = running_thread();			//�õ���ǰpcb�ĵ�ַ
	if (cur->status == TASK_RUNNING)
	{
		ASSERT(!find(&thread_ready_list, &cur->general_tag));     //Ŀǰ�����еĿ϶�ready_list�ǲ��ڵ�
		append(&thread_ready_list, &cur->general_tag);            //����β��

		cur->status = TASK_READY;
		cur->ticks = cur->priority;
	}
	else
	{
	}

	ASSERT(!empty(&thread_ready_list));
	struct task_struct* thread_tag = pop(&thread_ready_list);
	//��������е������ ������д��һ����������
	struct task_struct* next = (struct task_struct*)((uint32_t)thread_tag & 0xfffff000);
	next->status = TASK_RUNNING;
	process_activate(next);
	switch_to(cur, next);                                              //espͷ������ ���ص�ַ +12��next +8��cur
}

void thread_init(void)
{
	put_str("thread_init start!\n");
	list_init(&thread_ready_list);
	list_init(&thread_all_list);
	lock_init(&pid_lock);
	make_main_thread();
	idle_thread = thread_start("idle", 10, idle, NULL);
	put_str("thread_init done!\n");
}

void thread_block(enum task_status stat)
{
	//����block״̬�Ĳ��������������������µ�
	ASSERT(((stat == TASK_BLOCKED) || (stat == TASK_WAITING) || stat == TASK_HANGING));

	enum intr_status old_status = intr_disable();			 //���ж�
	struct task_struct* cur_thread = running_thread();
	cur_thread->status = stat;					 //��״̬��������

	//�������л����������� ��������status����running �����ٱ��ŵ�����������
	schedule();

	//���л�����֮���ٽ��е�ָ����
	intr_set_status(old_status);
}

//����ӵ������ִ�е� �����߰�ԭ�������������߳����·ŵ�������
void thread_unblock(struct task_struct* pthread)
{
	enum intr_status old_status = intr_disable();
	if ((pthread->status == TASK_BLOCKED) || (pthread->status == TASK_WAITING) || (pthread->status == TASK_HANGING)) {
		if (pthread->status != TASK_READY)
		{
			//�������߳� ��Ӧ�ô����ھ��������У�
			ASSERT(!find(&thread_ready_list, &pthread->general_tag));
			if (find(&thread_ready_list, &pthread->general_tag))
				PANIC("thread_unblock: blocked thread in ready_list\n"); //debug.h�ж����

			//�������˺ܾõ�������ھ���������ǰ��
			push(&thread_ready_list, &pthread->general_tag);

			//״̬��Ϊ����̬
			pthread->status = TASK_READY;
		}
	}
	intr_set_status(old_status);
}

//�����ó�CPU
void thread_yield(void) {
	struct task_struct* cur = running_thread();
	enum intr_status old_status = intr_disable();
	ASSERT(!find(&thread_ready_list, &cur->general_tag));
	append(&thread_ready_list, &cur->general_tag);
	old_status = TASK_READY;
	schedule();
	intr_set_status(old_status);
}