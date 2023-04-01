#ifndef __LIB_KERNEL_LIST_H
#define __LIB_KERNEL_LIST_H
#include "stdint.h"

#define offset(struct_type,member) (int) (&((struct_type*)0)->member)          //���Ǻ��������д����ʲô
#define elem2entry(struct_type,struct_member_name,elem_ptr) \
	(struct_type*)((int)elem_ptr - offset(struct_type,struct_member_name)) //Ҳ���Ǻ�������� ��������

struct list_elem
{
	struct list_elem* prev; //ǰ��Ľڵ�
	struct list_elem* next; //����Ľڵ�
};

struct list
{
	struct list_elem head; // ب�Ų����ͷ��
	struct list_elem tail; // ب�Ų����β��
};

typedef bool (function)(struct list_elem*, int arg);

void list_init(struct list*);
void insert(struct list_elem* before, struct list_elem* elem);
void push(struct list* plist, struct list_elem* elem);
void append(struct list* plist, struct list_elem* elem);
void remove(struct list_elem* pelem);
struct list_elem* pop(struct list* plist);
bool empty(struct list* plist);
uint32_t len(struct list* plist);
struct list_elem* traversal(struct list* plist, function func, int arg);
bool find(struct list* plist, struct list_elem* obj_elem);

#endif
