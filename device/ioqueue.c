#include "debug.h"
#include "ioqueue.h"
#include "interrupt.h"

void ioqueue_init(struct ioqueue* ioq) {
	lock_init(&ioq->lock);
	ioq->producer = ioq->consumer = NULL;
	ioq->head = ioq->tail = 0;
}

int32_t next_pos(int32_t pos) {
	return (pos + 1) % bufsize;
}

bool ioq_full(struct ioqueue* ioq) {
	ASSERT(intr_get_status() == INTR_OFF);
	return (ioq->tail - ioq->head) == 1;
}

bool ioq_empty(struct ioqueue* ioq) {
	ASSERT(intr_get_status() == INTR_OFF);
	return ioq->head == ioq->tail;
}

//ʹ��ǰ�������߻�����֮�ڻ������ϵȴ�
void ioq_wait(struct task_struct** waiter) {
	ASSERT(*waiter == NULL && waiter != NULL);
	*waiter == running_thread();
	thread_block(TASK_BLOCKED);
}

//���������߻�������
void ioq_wakeup(struct task_struct** waiter) {
	ASSERT(*waiter != NULL);
	thread_unblock(*waiter);
	*waiter = NULL;
}

char ioq_getchar(struct ioqueue* ioq) {
	//���ж��Ƿ�ر��ж�
	ASSERT(intr_get_status() == INTR_OFF);

	//�ж��Ƿ�Ϊ�գ����Ϊ����������Ҫ���ߵȴ�
	while (ioq_empty(ioq)) {
		try_lock(&ioq->lock);
		ioq_wait(&ioq->consumer);
		try_release(&ioq->lock);
	}

	//��ȡbuf�е�����
	char ch = ioq->buf[ioq->tail];
	ioq->tail = next_pos(ioq->tail);

	if (ioq->producer != NULL) {
		ioq_wakeup(&ioq->producer);
	}

	return ch;
}

void ioq_setchar(struct ioqueue* ioq, char byte) {
	//���ж��Ƿ�ر��ж�
	ASSERT(intr_get_status() == INTR_OFF);

	//�ж��Ƿ������������������������Ҫ���ߵȴ�
	while (ioq_full(ioq)) {
		try_lock(&ioq->lock);
		ioq_wait(&ioq->producer);
		try_release(&ioq->lock);
	}

	//��ȡbuf�е�����
	ioq->buf[ioq->head]=byte;
	ioq->head = next_pos(ioq->head);

	if (ioq->consumer != NULL) {
		ioq_wakeup(&ioq->consumer);
	}

	return byte;
}


