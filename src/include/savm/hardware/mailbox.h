#ifndef __SAVM_HARDWARE_MAILBOX_H_
#define __SAVM_HARDWARE_MAILBOX_H_ 1

#include <savm/error.h>
#include <stdint.h>
#include <string.h>

struct savm_mailbox_dev;
struct savm;

typedef uint64_t (*savm_mailbox_read_p)(struct savm_mailbox_dev* dev,uint64_t i);
typedef void (*savm_mailbox_write_p)(struct savm_mailbox_dev* dev,uint64_t i,uint64_t data);
typedef savm_error_e (*savm_mailbox_dev_create_p)(struct savm_mailbox_dev* dev);
typedef savm_error_e (*savm_mailbox_dev_destroy_p)(struct savm_mailbox_dev* dev);
typedef savm_error_e (*savm_mailbox_dev_reset_p)(struct savm_mailbox_dev* dev);
typedef savm_error_e (*savm_mailbox_dev_cycle_p)(struct savm_mailbox_dev* dev,struct savm* vm);

typedef struct {
	uint16_t vendorID;
	uint16_t deviceID;
	uint8_t rev;
} savm_mailbox_devhdr_t;

typedef struct savm_mailbox_dev {
	savm_mailbox_devhdr_t hdr;
	char name[128];
	size_t size;
	void* impl;
	
	savm_mailbox_read_p read;
	savm_mailbox_write_p write;
	savm_mailbox_dev_create_p create;
	savm_mailbox_dev_destroy_p destroy;
	savm_mailbox_dev_reset_p reset;
	savm_mailbox_dev_cycle_p cycle;
} savm_mailbox_dev_t;

typedef struct savm_mailbox {
	size_t devCount;
	savm_mailbox_dev_t* devices;
	
	uint64_t devIndex;
	uint64_t dataIndex;
} savm_mailbox_t;

savm_error_e savm_mailbox_create(savm_mailbox_t* mailbox);
savm_error_e savm_mailbox_destroy(savm_mailbox_t* mailbox);
savm_error_e savm_mailbox_reset(savm_mailbox_t* mailbox,struct savm* vm);
savm_error_e savm_mailbox_cycle(savm_mailbox_t* mailbox,struct savm* vm);
savm_error_e savm_mailbox_adddev(savm_mailbox_t* mailbox,savm_mailbox_dev_t dev);

#endif
