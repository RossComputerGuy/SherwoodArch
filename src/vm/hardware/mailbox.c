#include <savm/hardware/mailbox.h>
#include <savm/vm.h>
#include <stdlib.h>

uint64_t savm_mailbox_read(savm_t* vm,uint64_t i);
void savm_mailbox_write(savm_t* vm,uint64_t i,uint64_t data);

savm_error_e savm_mailbox_create(savm_mailbox_t* mailbox) {
	memset(mailbox,0,sizeof(savm_mailbox_t));
	return SAVM_ERROR_NONE;
}

savm_error_e savm_mailbox_destroy(savm_mailbox_t* mailbox) {
	if(mailbox->devices != NULL) {
		for(size_t i = 0;i < mailbox->devCount;i++) {
			if(mailbox->devices[i].destroy != NULL) {
				savm_error_e err = mailbox->devices[i].destroy(&mailbox->devices[i]);
				if(err != SAVM_ERROR_NONE) return err;
			}
		}
		free(mailbox->devices);
	}
	return SAVM_ERROR_NONE;
}

savm_error_e savm_mailbox_reset(savm_mailbox_t* mailbox,savm_t* vm) {
	savm_error_e err = savm_ioctl_mmap(vm,SAVM_IO_MAILBOX_BASE,SAVM_IO_MAILBOX_END,savm_mailbox_read,savm_mailbox_write);
	for(size_t i = 0;i < mailbox->devCount;i++) {
		if(mailbox->devices[i].reset != NULL) {
			err = mailbox->devices[i].reset(&mailbox->devices[i]);
			if(err != SAVM_ERROR_NONE) return err;
		}
	}
	return SAVM_ERROR_NONE;
}

savm_error_e savm_mailbox_cycle(savm_mailbox_t* mailbox,struct savm* vm) {
	for(size_t i = 0;i < mailbox->devCount;i++) {
		if(mailbox->devices[i].cycle != NULL) {
			savm_error_e err = mailbox->devices[i].cycle(&mailbox->devices[i],vm);
			if(err != SAVM_ERROR_NONE) return err;
		}
	}
	return SAVM_ERROR_NONE;
}

savm_error_e savm_mailbox_adddev(savm_mailbox_t* mailbox,savm_mailbox_dev_t dev) {
	size_t i = -1;
	if(mailbox->devCount < 1 || mailbox->devices == NULL) {
		mailbox->devCount = 1;
		mailbox->devices = malloc(sizeof(savm_mailbox_dev_t));
		i = 0;
	}
	if(i == -1) {
		i = mailbox->devCount++;
		mailbox->devices = realloc(mailbox->devices,sizeof(savm_mailbox_dev_t)*mailbox->devCount);
	}
	if(mailbox->devices == NULL) return SAVM_ERROR_MEM;
	memcpy(&mailbox->devices[i],&dev,sizeof(savm_mailbox_dev_t));
	if(mailbox->devices[i].create != NULL) return mailbox->devices[i].create(&mailbox->devices[i]);
	return SAVM_ERROR_NONE;
}

uint64_t savm_mailbox_read(savm_t* vm,uint64_t i) {
	switch(i) {
		case 0: return vm->mailbox.devCount;
		case 1: return vm->mailbox.devIndex;
		case 2: return vm->mailbox.dataIndex;
		case 3:
			if(vm->mailbox.devIndex >= 0 && vm->mailbox.devIndex < vm->mailbox.devCount) {
				if(vm->mailbox.devices[vm->mailbox.devIndex].read != NULL) return vm->mailbox.devices[vm->mailbox.devIndex].read(&vm->mailbox.devices[vm->mailbox.devIndex],vm->mailbox.dataIndex);
			} else return 0xFF;
		case 4:
			if(vm->mailbox.devIndex >= 0 && vm->mailbox.devIndex < vm->mailbox.devCount) {
				return vm->mailbox.devices[vm->mailbox.devIndex].hdr.vendorID;
			} else return 0xFFFF;
		case 5:
			if(vm->mailbox.devIndex >= 0 && vm->mailbox.devIndex < vm->mailbox.devCount) {
				return vm->mailbox.devices[vm->mailbox.devIndex].hdr.deviceID;
			} else return 0xFFFF;
		case 6:
			if(vm->mailbox.devIndex >= 0 && vm->mailbox.devIndex < vm->mailbox.devCount) {
				return vm->mailbox.devices[vm->mailbox.devIndex].hdr.rev;
			} else return 0xFFFF;
		case 7:
			if(vm->mailbox.devIndex >= 0 && vm->mailbox.devIndex < vm->mailbox.devCount) {
				return vm->mailbox.devices[vm->mailbox.devIndex].hdr.type;
			} else return 0xFFFF;
		case 8:
			if(vm->mailbox.devIndex >= 0 && vm->mailbox.devIndex < vm->mailbox.devCount) {
				return vm->mailbox.devices[vm->mailbox.devIndex].hdr.classCode;
			} else return 0xFFFF;
		case 9:
			if(vm->mailbox.devIndex >= 0 && vm->mailbox.devIndex < vm->mailbox.devCount) {
				return vm->mailbox.devices[vm->mailbox.devIndex].hdr.subclass;
			} else return 0xFFFF;
	}
	return 0;
}

void savm_mailbox_write(savm_t* vm,uint64_t i,uint64_t data) {
	switch(i) {
		case 1:
			vm->mailbox.devIndex = data;
			break;
		case 2:
			vm->mailbox.dataIndex = data;
			break;
		case 3:
			if(vm->mailbox.devIndex >= 0 && vm->mailbox.devIndex < vm->mailbox.devCount) {
				if(vm->mailbox.devices[vm->mailbox.devIndex].write != NULL) vm->mailbox.devices[vm->mailbox.devIndex].write(&vm->mailbox.devices[vm->mailbox.devIndex],vm->mailbox.dataIndex,data);
			}
			break;
	}
}
