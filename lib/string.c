#include "string.h"
#include "global.h"
#include "debug.h"

/*��dst_��ַ��ʼ�ĺ�sizeλ��Ϊvalue*/
void memset(void *dst_, uint8_t value, uint32_t size) {
	ASSERT(dst_ != NULL);
	uint8_t* dst = (uint8_t*)dst_;
	while (size-- > 0) {
		*(dst++) = value;
	}
}

/*��src_��ַ��sizeλ��ֵ���Ƶ�dst_��*/
void memcpy(void *dst_, const void *src_, uint32_t size) {
	ASSERT(dst_ != NULL);
	ASSERT(src_ != NULL);
	uint8_t* dst = dst_;
	const uint8_t* src = src_;
	while (size-- > 0) {
		*(dst++) = *(src++);
	}
}

/*�Ƚϣ���������Ϊ0��a>bΪ1��a<bΪ-1*/
int memcmp(const void* a_, const void* b_, uint32_t size) {
	const char* a = a_;
	const char* b = b_;
	ASSERT(a != NULL || b != NULL);
	while (size-- > 0) {
		if (*a > *b) {
			return 1;
		}
		else if (*a < *b) {
			return -1;
		}
		a++;
		b++;
	}
	return 0;
}

/*�ַ�����src_���Ƶ�dst_*/
char* strcpy(char* dst_, const char* src_) {
	ASSERT(dst_ != NULL && src_ != NULL);
	char* dst = dst_;
	while ((*(dst_++) = *(src_++)));
	return dst;
}

/*�����ַ�������*/
uint32_t strlen(const char* str_) {
	ASSERT(str_ != NULL);
	const char* p = str_;
	while (*(p++));
	return p - str_ - 1;
}

/*�Ƚ������ַ���,a=b����0,a>b����1,a<b����-1*/
int8_t strcmp(const char* a, const char* b) {
	ASSERT(a != NULL && b != NULL);
	while (*a != 0 && *a == *b) {
		a++;
		b++;
	}
	return *a<*b ? -1 : *a>*b;
}

/*��ǰ�����ҵ�ch��str���״γ��ֵ�λ��*/
char* strch(const char* str, const uint8_t ch) {
	ASSERT(str != NULL);
	while (*str!=0)
	{
		if (*str == ch) {
			return (char*)str;
		}
		str++;
	}
	return NULL;
}

/*�Ӻ���ǰ�ҵ�ch��str���״γ��ֵ�λ��*/
char* strrch(const char* str, const uint8_t ch) {
	ASSERT(str != NULL);
	const char* chr = NULL;

	while (*str != 0) {
		if (*str == ch) {
			chr = str;
		}
		str++;
	}
	return (char*)chr;
}

/*���ַ���src_ƴ�ӵ�dst��*/
char* strcat(char* dst_, const char* src_) {
	ASSERT(dst_ != NULL && src_ != NULL);
	char* dst = dst_;
	while (*(dst_++));
	--dst_;
	while ((*(dst_++) = *(src_++)));
	return dst;
}

/*����ڴ渲�����⣬��ƴ�ӵ��ַ������·���һ����ַ*/
char* newstrcat(char* dst_, const char* src_) {
	ASSERT(dst_ != NULL && src_ != NULL);
	char* dst = "";
	char* p = dst;
	while (*(p++)=*(dst_++));
	--p;
	while ((*(p++) = *(src_++)));
	return dst;
}

/*���ַ���str�в����ַ�ch���ֵĴ���*/
uint32_t strchrs(const char* str, uint8_t ch) {
	ASSERT(str != NULL);
	uint32_t count = 0;
	while (*str != 0) {
		if (*(str++) == ch) {
			count++;
		}
	}
	return count;
}