#include "memory.h"
#include "../lib/stdint.h"
#include "print.h"
#include "bitmap.h"
#include "debug.h"
#include "string.h"
#include "../thread/sync.h"
#include "../thread/thread.h"
#include "global.h"
#include "interrupt.h"
#define PG_SIZE 4096
#define MEM_BITMAP_BASE 0Xc009a000   //λͼ��ʼ��ŵ�λ��
#define K_HEAP_START    0xc0100000   //�ں�ջ��ʼλ��
#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)

struct pool
{
	struct bitmap pool_bitmap; //λͼ�������ڴ�ʹ��
	uint32_t phy_addr_start;   //�ڴ�ؿ�ʼ����ʼ��ַ
	uint32_t pool_size;	//������
	struct lock lock;
};
struct pool kernel_pool, user_pool; //�����ں��ڴ�� �� �û��ڴ��
struct virtual_addr kernel_vaddr;    //�ں������ڴ������

/*�ڴ�ֿ�*/
struct arena {
	struct mem_block_desc* desc; //������ָ�룬ָ���Լ���Ӧ���ڴ�������
	uint32_t cnt;
	bool large;
};

struct mem_block_desc k_block_descs[DESC_CNT];

/** ----------�ڴ�ֿ����ģ��-----------**/
//��ʼ���ڴ��������
void block_desc_init(struct mem_block_desc* desc_array) {
	uint16_t desc_index, block_size = 16;
	for (desc_index = 0;desc_index < DESC_CNT;desc_index++) {
		desc_array[desc_index].block_size = block_size;
		desc_array[desc_index].block_per_arena = (PG_SIZE - sizeof(struct arena)) / block_size;

		list_init(&desc_array[desc_index].free_list);
		block_size = block_size << 1;
	}
}

//����arena�� ��idx����ڴ���ַ
static struct mem_block* arena2block(struct arena* a, uint32_t idx) {
	return (struct mem_block*)((uint32_t)a + sizeof(struct arena) + idx * a->desc->block_size);
}

/*�����ڴ��b���ڵ�arena�ĵ�ַ*/
static struct arena* block2arena(struct mem_block* b) {
	return (struct arena*)((uint32_t)b & 0xfffff000);
}

void* sys_malloc(uint32_t size) {
	enum pool_flags PF;
	struct pool* mem_pool;
	uint32_t pool_size;
	struct mem_block_desc* descs;
	struct task_struct* cur_thread = running_thread();

	/* �ж�ʹ���ĸ��ڴ�� */
	if (cur_thread->pgdir == NULL) { //�ں��߳�
		PF = PF_KERNEL;
		mem_pool = &kernel_pool;
		pool_size = kernel_pool.pool_size;
		descs = k_block_descs;
	}
	else {
		PF = PF_USER;
		mem_pool = &user_pool;
		pool_size = user_pool.pool_size;
		descs = cur_thread->u_block_desc;
	}

	/*��������ڴ治�������ط�Χ�ڣ��򷵻�NULL*/
	if (!(size > 0 && size < pool_size)) {
		return NULL;
	}
	struct arena* a;
	struct mem_block* b;
	try_lock(&mem_pool->lock);

	/*��������ڴ�� 1024�ͷ���ҳ��*/
	if (size > 1024) {
		uint32_t pg_cnt = DIV_ROUND_UP(size + sizeof(struct arena), PG_SIZE);
		a = malloc_page(PF, pg_cnt);
		if (a != NULL) {
			memset(a, 0, pg_cnt*PG_SIZE);
			a->desc = NULL;
			a->cnt = pg_cnt;
			a->large = true;
			try_release(&mem_pool->lock);
			return (void*)(a + 1); //���arena��Ԫ��Ϣ����ʣ���ڴ��ַ
		}
		else {
			try_release(&mem_pool->lock);
			return NULL;
		}
	}/* ����С�� 1024���ڴ��*/
	else {
		uint8_t desc_idx;

		//��С�����ң��ҵ��󷵻�
		for (desc_idx = 0;desc_idx < DESC_CNT;desc_idx++) {
			if (size <= descs[desc_idx].block_size) {
				break;
			}
		}

		/*���û�п��õ�arena�򴴽�һ��arena*/
		if (empty(&descs[desc_idx].free_list)) {
			a = malloc_page(PF, 1);
			if (a == NULL) {
				try_release(&mem_pool->lock);
				return NULL;
			}
			memset(a, 0, PG_SIZE);
			a->desc = &descs[desc_idx];
			a->large = false;
			a->cnt = descs[desc_idx].block_per_arena;
			uint32_t block_idx;
			enum intr_status old_status = intr_disable();

			/*��arena��ֳ��ڴ�飬���ӵ��ڴ����������free_list��*/
			for (block_idx = 0;block_idx < descs[desc_idx].block_per_arena;block_idx++) {
				b = arena2block(a, block_idx);
				ASSERT(!find(&a->desc->free_list, &b->free_elem));
				append(&a->desc->free_list,&b->free_elem);
			}
			intr_set_status(old_status);
		}

		/*��ʼ�����ڴ��*/
		b = elem2entry(struct mem_block, free_elem, pop(&descs[desc_idx].free_list));//����mem_block��free_elem�ҵ�mem_block��λ��
		memset(b, 0, descs[desc_idx].block_size);

		a = block2arena(b);
		a->cnt--;
		try_release(&mem_pool->lock);
		return (void*)b;
	}

}

/** ----------�ڴ����ģ��-----------**/

/*����������ַ*/
void pfree(uint32_t pg_phy_addr) {
	struct pool* mem_pool;
	uint32_t bit_idx = 0;
	if (pg_phy_addr >= user_pool.phy_addr_start) { //�û��ڴ�ع���
		mem_pool = &user_pool;
		bit_idx = (pg_phy_addr - user_pool.phy_addr_start) / PG_SIZE;
	}
	else {
		mem_pool = &kernel_pool;					//�ں��ڴ�ع���
		bit_idx = (pg_phy_addr - kernel_pool.phy_addr_start) / PG_SIZE;
	}
	bitmap_set(&mem_pool->pool_bitmap, bit_idx, 0);
}

/*����ҳ�������ַvaddr��ӳ�䣬ȥ��vaddr��Ӧ��pte*/
static void page_table_pte_remove(uint32_t vaddr) {
	uint32_t* pte = pte_ptr(vaddr);
	*pte &= ~PG_P_1;
	asm volatile("invlpg %0"::"m" (vaddr) : "memory");//����TLB��
}

/*���������ַ��*/
static void vaddr_remove(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt) {
	uint32_t bit_idx_start = 0, vaddr = (uint32_t)_vaddr, cnt = 0;
	if (pf == PF_KERNEL) {
		bit_idx_start = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
		while (cnt < pg_cnt) {
			bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + (cnt++), 0);
		}
	}
	else {
		struct task_struct* cur_thread = running_thread();
		bit_idx_start = (vaddr - cur_thread->userprog_vaddr.vaddr_start) / PG_SIZE;
		while (cnt < pg_cnt) {
			bitmap_set(&cur_thread->userprog_vaddr.vaddr_bitmap, bit_idx_start + (cnt++), 0);
		}
	}
}

/*�Ƿ��������ַvaddrΪ��ʼ��cnt������ҳ��*/
void mfree_page(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt) {
	uint32_t pg_phy_addr;
	uint32_t vaddr = (int32_t)_vaddr, page_cnt = 0;
	ASSERT(pg_cnt >= 1 && vaddr % PG_SIZE == 0);
	pg_phy_addr = addr_v2p(vaddr);

	ASSERT((pg_phy_addr%PG_SIZE) == 0 && pg_phy_addr >= 0x102000); //ȷ���ͷŵ������ڴ��ڵض�1MB+1KB��ҳĿ¼+1KB��С��ҳ����ַ��Χ��

	if (pg_phy_addr >= user_pool.phy_addr_start) {
		vaddr -= PG_SIZE;
		while (page_cnt < pg_cnt) {
			vaddr += PG_SIZE;
			pg_phy_addr = addr_v2p(vaddr);

			/*ȷ��������ַ�����û������ڴ��*/
			ASSERT((pg_phy_addr%PG_SIZE) == 0 && pg_phy_addr >= user_pool.phy_addr_start);

			pfree(pg_phy_addr);
			page_table_pte_remove(vaddr);

			page_cnt++;
		}
		vaddr_remove(pf, _vaddr, pg_cnt);
	}
	else {
		vaddr -= PG_SIZE;
		while (page_cnt < pg_cnt) {
			vaddr += PG_SIZE;
			pg_phy_addr = addr_v2p(vaddr);

			/*ȷ��������ַ�����û������ڴ��*/
			ASSERT((pg_phy_addr%PG_SIZE) == 0 && pg_phy_addr >= kernel_pool.phy_addr_start && pg_phy_addr < user_pool.phy_addr_start);

			pfree(pg_phy_addr);
			page_table_pte_remove(vaddr);

			page_cnt++;
		}
		vaddr_remove(pf, _vaddr, pg_cnt);
	}
}

void sys_free(void* ptr) {
	ASSERT(ptr != NULL);
	if (ptr != NULL) {
		enum pool_flags PF;
		struct pool* mem_pool;
		
		/*�ж����̻߳��ǽ���*/
		if (running_thread()->pgdir == NULL) {
			ASSERT((uint32_t)ptr >= K_HEAP_START);
			PF = PF_KERNEL;
			mem_pool = &kernel_pool;
		}
		else {
			PF = PF_USER;
			mem_pool = &user_pool;
		}

		try_lock(&mem_pool->lock);
		struct mem_block* b = ptr;
		struct arena* a = block2arena(b);

		ASSERT(a->large == 0 || a->large == 1);
		if (a->desc == NULL && a->large == true) {/*����ڴ���ֱ�ӻ�������arena*/
			mfree_page(PF, a, a->cnt);
		}
		else {
			/*С���ڴ���Ȼ���һ����*/
			append(&a->desc->free_list, &b->free_elem);
			/*�������ڴ������ ���� a��������ֱ�ӻ���arena*/
			if (++a->cnt == a->desc->block_per_arena) {
				uint32_t block_idx;
				for (block_idx = 0;block_idx < a->desc->block_per_arena;block_idx++) {
					struct mem_block* b = arena2block(a, block_idx);
					ASSERT(find(&a->desc->free_list, &b->free_elem));
					remove(&b->free_elem);
				}
				mfree_page(PF, a, 1);
			}
		}
		try_release(&mem_pool->lock);
	}

}
/** ----------�ڴ����ģ��-----------**/
void* vaddr_get(enum pool_flags pf, uint32_t pg_cnt)
{
	int vaddr_start = 0, bit_idx_start = -1;
	uint32_t cnt = 0;
	if (pf == PF_KERNEL)
	{
		bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap, pg_cnt);
		if (bit_idx_start == -1)	return NULL;
		while (cnt < pg_cnt)
			bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + (cnt++), 1);
		vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
	}
	else
	{
		struct task_struct* cur = running_thread();
		bit_idx_start = bitmap_scan(&cur->userprog_vaddr.vaddr_bitmap, pg_cnt);
		if (bit_idx_start == -1)	return NULL;

		while (cnt < pg_cnt)
			bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, bit_idx_start + (cnt++), 1);
		vaddr_start = cur->userprog_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
		ASSERT((uint32_t)vaddr_start < (0xc0000000 - PG_SIZE));
	}
	return (void*)vaddr_start;
}


uint32_t* pte_ptr(uint32_t vaddr)
{
	uint32_t* pte = (uint32_t*)(0xffc00000 + ((vaddr & 0xffc00000) >> 10) + PTE_IDX(vaddr) * 4);
	return pte;
}

uint32_t* pde_ptr(uint32_t vaddr)
{
	uint32_t* pde = (uint32_t*)((0xfffff000) + PDE_IDX(vaddr) * 4);
	return pde;
}

void* palloc(struct pool* m_pool)
{
	int bit_idx = bitmap_scan(&m_pool->pool_bitmap, 1);
	if (bit_idx == -1)	return NULL;
	bitmap_set(&m_pool->pool_bitmap, bit_idx, 1);
	uint32_t page_phyaddr = ((bit_idx * PG_SIZE) + m_pool->phy_addr_start);
	return (void*)page_phyaddr;
}

void page_table_add(void* _vaddr, void* _page_phyaddr)
{
	uint32_t vaddr = (uint32_t)_vaddr, page_phyaddr = (uint32_t)_page_phyaddr;
	uint32_t* pde = pde_ptr(vaddr);
	uint32_t* pte = pte_ptr(vaddr);

	if (*pde & 0x00000001)
	{
		ASSERT(!(*pte & 0x00000001));
		if (!(*pte & 0x00000001))
			*pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
		else
		{
			PANIC("pte repeat");
			*pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
		}
	}
	else
	{
		uint32_t pde_phyaddr = (uint32_t)palloc(&kernel_pool);
		*pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
		memset((void*)((int)pte & 0xfffff000), 0, PG_SIZE);
		ASSERT(!(*pte & 0x00000001));
		*pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
	}
	return;
}

void* malloc_page(enum pool_flags pf, uint32_t pg_cnt)
{
	ASSERT(pg_cnt > 0 && pg_cnt < 3840);

	void* vaddr_start = vaddr_get(pf, pg_cnt);
	if (vaddr_start == NULL)	return NULL;


	uint32_t vaddr = (uint32_t)vaddr_start, cnt = pg_cnt;
	struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;

	while (cnt-- > 0)
	{
		void* page_phyaddr = palloc(mem_pool);
		if (page_phyaddr == NULL)	return NULL;
		page_table_add((void*)vaddr, page_phyaddr);
		vaddr += PG_SIZE;
	}
	return vaddr_start;
}

void* get_kernel_pages(uint32_t pg_cnt)
{
	void* vaddr = malloc_page(PF_KERNEL, pg_cnt);
	if (vaddr != NULL)	memset(vaddr, 0, pg_cnt*PG_SIZE);
	return vaddr;
}

void* get_user_pages(uint32_t pg_cnt)
{
	try_lock(&user_pool.lock);	//�û����̿��ܻ������ͻ �󲿷�ʱ�䶼���û����� �ں˽��̿���������������ͻ
	void* vaddr = malloc_page(PF_USER, pg_cnt);
	if (vaddr != NULL)	memset(vaddr, 0, pg_cnt*PG_SIZE);
	try_release(&user_pool.lock);
	return vaddr;
}

void* get_a_page(enum pool_flags pf, uint32_t vaddr)
{
	struct pool* mem_pool = (pf == PF_KERNEL) ? &kernel_pool : &user_pool;
	try_lock(&mem_pool->lock);

	struct task_struct* cur = running_thread();
	int32_t bit_idx = -1;

	//�����ַλͼ��1
	if (cur->pgdir != NULL && pf == PF_USER)
	{
		bit_idx = (vaddr - cur->userprog_vaddr.vaddr_start) / PG_SIZE;
		ASSERT(bit_idx > 0);
		bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, bit_idx, 1);
	}
	else if (cur->pgdir == NULL && pf == PF_KERNEL)
	{
		bit_idx = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
		ASSERT(bit_idx > 0);
		bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx, 1);
	}
	else
		PANIC("get_a_page:not allow kernel alloc userspace or user alloc kernelspace by get_a_page");

	void* page_phyaddr = palloc(mem_pool);
	if (page_phyaddr == NULL)
		return NULL;
	page_table_add((void*)vaddr, page_phyaddr);
	try_release(&mem_pool->lock);
	return (void*)vaddr;
}

/* �õ������ַӳ���������ַ */
uint32_t addr_v2p(uint32_t vaddr)
{
	uint32_t* pte = pte_ptr(vaddr);
	return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
}

void mem_pool_init(uint32_t all_mem)
{
	put_str("    mem_pool_init start!\n");
	uint32_t page_table_size = PG_SIZE * 256;       //ҳ��ռ�õĴ�С
	uint32_t used_mem = page_table_size + 0x100000; //�Ͷ�1MB���ڴ� + ҳ����ռ�õĴ�С
	uint32_t free_mem = all_mem - used_mem;

	uint16_t all_free_pages = free_mem / PG_SIZE;   //�����ҳ�� = �ܿ����ڴ� / һҳ�Ĵ�С

	uint16_t kernel_free_pages = all_free_pages / 2; //�ں� �� �û� ��ƽ��ʣ���ڴ�
	uint16_t user_free_pages = all_free_pages - kernel_free_pages; //��һ������ �ͻ���1 ��ȥ����

	//kbm kernel_bitmap ubm user_bitmap
	uint32_t kbm_length = kernel_free_pages / 8;    //һλ���ɱ�ʾһҳ 8λһ����
	uint32_t ubm_length = user_free_pages / 8;

	//kp kernel_pool up user_pool
	uint32_t kp_start = used_mem;
	uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE;

	kernel_pool.phy_addr_start = kp_start;
	user_pool.phy_addr_start = up_start;

	kernel_pool.pool_size = kernel_free_pages * PG_SIZE;
	user_pool.pool_size = user_free_pages * PG_SIZE;

	kernel_pool.pool_bitmap.bits = (void*)MEM_BITMAP_BASE;
	user_pool.pool_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length);

	kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length;
	user_pool.pool_bitmap.btmp_bytes_len = ubm_length;

	put_str("        kernel_pool_bitmap_start:");
	put_int((int)kernel_pool.pool_bitmap.bits);
	put_str(" kernel_pool_phy_addr_start:");
	put_int(kernel_pool.phy_addr_start);
	put_char('\n');
	put_str("        user_pool_bitmap_start:");
	put_int((int)user_pool.pool_bitmap.bits);
	put_str(" user_pool_phy_addr_start:");
	put_int(user_pool.phy_addr_start);
	put_char('\n');

	bitmap_init(&kernel_pool.pool_bitmap);
	bitmap_init(&user_pool.pool_bitmap);

	kernel_vaddr.vaddr_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length + ubm_length);
	kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length;

	kernel_vaddr.vaddr_start = K_HEAP_START;
	bitmap_init(&kernel_vaddr.vaddr_bitmap);
	lock_init(&kernel_pool.lock);
	lock_init(&user_pool.lock);
	put_str("    mem_pool_init done\n");
	return;
}

void mem_init()
{
	put_str("mem_init start!\n");
	uint32_t mem_bytes_total = (*(uint32_t*)(0xb00)); //���ǰ����ڴ��ֵ������0x800 ��֮ǰΪ����ʾ�Ƚ϶��� ������0x800����
	mem_pool_init(mem_bytes_total);
	block_desc_init(k_block_descs);
	put_str("mem_init done!\n");
	return;
}