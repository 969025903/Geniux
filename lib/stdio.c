#include "stdio.h"
#include "stdint.h"
#include "string.h"
#include "syscall.h"


void itoa(uint32_t value, char** buf_ptr_addr, uint8_t base)//����������� Ϊ���޸�һ��ָ���ֵ ��ȡһ��& �����޸�һ��ָ�뱾����ֵ
{
	uint32_t m = value % base;
	uint32_t i = value / base;  //����Ϊ0�����λ�� ������� û�����������
	if (i)
		itoa(i, buf_ptr_addr, base);
	if (m < 10)                  //mС��10����
		*((*buf_ptr_addr)++) = m + '0';
	else			 //m����10����
		*((*buf_ptr_addr)++) = m + 'A' - 10;
}

uint32_t vsprintf(char* str, const char* format, va_list ap)
{
	char* buf_ptr = str;
	const char* index_ptr = format;
	char index_char = *index_ptr;
	int32_t arg_int;
	char* arg_str;
	while (index_char)		//���������ַ���Ū
	{
		if (index_char != '%')
		{
			*(buf_ptr++) = index_char;
			index_char = *(++index_ptr);
			continue;
		}
		index_char = *(++index_ptr);
		switch (index_char)
		{
		case 's':
			arg_str = va_arg(ap, char*);
			strcpy(buf_ptr, arg_str);
			buf_ptr += strlen(arg_str);
			index_char = *(++index_ptr);
			break;
		case 'x':
			arg_int = va_arg(ap, int);
			itoa(arg_int, &buf_ptr, 16);
			index_char = *(++index_ptr);
			break;
		case 'd':
			arg_int = va_arg(ap, int);
			if (arg_int < 0)
			{
				arg_int = 0 - arg_int;
				*(buf_ptr++) = '-';
			}
			itoa(arg_int, &buf_ptr, 10);
			index_char = *(++index_ptr);
			break;
		case 'c':
			*(buf_ptr++) = va_arg(ap, char);
			index_char = *(++index_ptr);
		}
	}
	return strlen(str);
}

uint32_t printf(const char* format, ...)
{
	va_list args;
	uint32_t retval;
	va_start(args, format);		//argsָ��char* ��ָ�� ����ָ����һ��ջ����
	char buf[1024] = { 0 };
	retval = vsprintf(buf, format, args);
	va_end(args);
	write(buf);
	return retval;
}

uint32_t sprintf(char* _des, const char* format, ...)
{
	va_list args;
	uint32_t retval;
	va_start(args, format);		//argsָ��char* ��ָ�� ����ָ����һ��ջ����
	retval = vsprintf(_des, format, args);
	va_end(args);
	return retval;
}
