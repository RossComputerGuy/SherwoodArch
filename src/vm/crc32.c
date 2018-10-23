#include <savm/crc32.h>

unsigned int crc32(const unsigned char* buf,int len,unsigned int init) {
	unsigned int crc = init;
	while(len--) {
		crc = (crc << 8) ^ crc32_table[((crc >> 24) ^ *buf) & 255];
		buf++;
	}
	return crc;
}
