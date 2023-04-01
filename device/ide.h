#ifndef __DEVICE_IDE_H
#define __DEVICE_IDE_H

#include "stdint.h"
#include "bitmap.h"
#include "list.h"
#include "../thread/sync.h"
#include "list.h"

//�����ṹ
struct partition
{
	uint32_t start_lba;          //��ʼ����
	uint32_t sec_cnt;            //������
	struct disk* my_disk;        //��������Ӳ��
	struct list_elem part_tag;   //���ڶ����еı��
	char name[8];                //��������
	struct super_block* sb;	  //������ ������
	struct bitmap block_bitmap;  //��λͼ
	struct bitmap inode_bitmap;  //i���λͼ
	struct list open_inodes;     //��������
};

struct partition_table_entry
{
	uint8_t bootable;		  //�Ƿ������
	uint8_t start_head;	  //��ʼ��ͷ��
	uint8_t start_sec;		  //��ʼ������
	uint8_t start_chs;		  //��ʼ�����
	uint8_t fs_type;		  //��������
	uint8_t end_head;		  //������ͷ��
	uint8_t end_sec;		  //����������
	uint8_t end_chs;		  //���������
	uint32_t start_lba;	  //��������ʼ��lba��ַ
	uint32_t sec_cnt;		  //��������Ŀ
} __attribute__((packed));

struct boot_sector
{
	uint8_t other[446];	  			//446 + 64 + 2 446������ռλ�õ�
	struct partition_table_entry partition_table[4];  //��������4�� 64�ֽ�
	uint16_t signature;				//���ı�ʶ��־ ħ��0x55 0xaa				
} __attribute__((packed));			//c++�Ըýṹ����ѹ����������������

//Ӳ��
struct disk
{
	char name[8];		      //��Ӳ�̵�����
	struct ide_channel* my_channel;    //���Ӳ�̹������ĸ�ideͨ��
	uint8_t dev_no;		      //0��ʾ���� 1��ʾ����
	struct partition prim_parts[4];  //������������4��
	struct partition logic_parts[8]; //�߼��������֧��8��
};

// ata ͨ���ṹ
struct ide_channel
{
	char name[8];		      //ataͨ������
	uint16_t port_base;	      //��ͨ������ʼ�˿ں�
	uint8_t  irq_no;		      //��ͨ�����õ��жϺ�
	struct lock lock;                //ͨ���� һ��Ӳ��һͨ�� ����ͬʱ
	bool expecting_intr;	      //�ȴ�Ӳ���жϵ�bool
	struct semaphore disk_done;      //�������� ������������  ������һ�� ���Լ������� ��cpu�ڳ���
	struct disk devices[2];	      //һͨ��2Ӳ�� 1��1��
};

void ide_init(void);
void select_disk(struct disk* hd);
void select_sector(struct disk* hd, uint32_t lba, uint8_t sec_cnt);
void cmd_out(struct ide_channel* channel, uint8_t cmd);
void read_from_sector(struct disk* hd, void* buf, uint8_t sec_cnt);
void write2sector(struct disk* hd, void* buf, uint8_t sec_cnt);
bool busy_wait(struct disk* hd);
void ide_read(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt);
void ide_write(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt);
void intr_hd_handler(uint8_t irq_no);
void swap_pairs_bytes(const char* dst, char* buf, uint32_t len);
void identify_disk(struct disk* fd);
void partition_scan(struct disk* hd, uint32_t ext_lba);
bool partition_info(struct list_elem* pelem, int arg);

#endif
