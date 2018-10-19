#include <savm/hardware/rtc.h>
#include <savm/vm.h>

uint64_t savm_rtc_read(savm_t* vm,uint64_t i);
void savm_rtc_write(savm_t* vm,uint64_t i,uint64_t data);

savm_error_e savm_rtc_create(savm_rtc_t* rtc) {
	memset(rtc,0,sizeof(savm_rtc_t));
	return SAVM_ERROR_NONE;
}

savm_error_e savm_rtc_destroy(savm_rtc_t* rtc) {
	if(rtc->timers != NULL) free(rtc->timers);
	return SAVM_ERROR_NONE;
}

savm_error_e savm_rtc_reset(savm_rtc_t* rtc,savm_t* vm) {
	savm_error_e err = savm_ioctl_mmap(vm,SAVM_IO_RTC_BASE,SAVM_IO_RTC_END,savm_rtc_read,savm_rtc_write);
	if(err != SAVM_ERROR_NONE) return err;
	
	if(rtc->timers != NULL) free(rtc->timers);
	rtc->timerCount = 0;
	return SAVM_ERROR_NONE;
}

savm_error_e savm_rtc_cycle(savm_rtc_t* rtc,struct savm* vm) {
	for(size_t i = 0;i < rtc->timerCount;i++) {
		rtc->timers[i].since = clock()*1000/CLOCKS_PER_SEC;
		if(rtc->timers[i].since <= rtc->timers[i].end && !(vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_INTR)) {
			vm->cpu.cores[vm->cpu.currentCore].regs.data[0] = i;
			savm_error_e err = savm_cpu_intr(vm,SAVM_CPU_INT_TIMER);
			if(err != SAVM_ERROR_NONE) return err;
			vm->rtc.timers[i].finished = 1;
		}
	}
	return SAVM_ERROR_NONE;
}

uint64_t savm_rtc_read(savm_t* vm,uint64_t i) {
	time_t rawtime;
	time(&rawtime);
	
	struct tm* timeinfo = localtime(&rawtime);
	
	switch(i) {
		case 0: return timeinfo->tm_sec;
		case 1: return timeinfo->tm_min;
		case 2: return timeinfo->tm_hour;
		case 3: return timeinfo->tm_mday;
		case 4: return timeinfo->tm_mon;
		case 5: return timeinfo->tm_year;
		case 6: return timeinfo->tm_isdst;
		
		case 7: return vm->rtc.timerCount;
	}
	return 0;
}

void savm_rtc_write(savm_t* vm,uint64_t i,uint64_t data) {
	switch(i) {
		case 0 ... 6:
			/* REAL HARDWARE: this would write the time values (in the same order as reading) to the RTC */
			break;
		case 7:
			{
				size_t i = -1;
				if(vm->rtc.timerCount < 1 || vm->rtc.timers == NULL) {
					vm->rtc.timerCount = 1;
					vm->rtc.timers = malloc(sizeof(savm_rtc_timer_t));
					i = 0;
				}
				for(size_t x = 0;x < vm->rtc.timerCount;x++) {
					if(vm->rtc.timers[x].finished) {
						i = x;
						break;
					}
				}
				if(i == -1) {
					i = vm->rtc.timerCount++;
					vm->rtc.timers = realloc(vm->rtc.timers,sizeof(savm_rtc_timer_t)*vm->rtc.timerCount);
				}
				if(vm->rtc.timers == NULL) break;
				vm->rtc.timers[i].since = clock()*1000/CLOCKS_PER_SEC;
				vm->rtc.timers[i].end = vm->rtc.timers[i].since+data;
				vm->rtc.timers[i].finished = 0;
			}
			break;
	}
}
