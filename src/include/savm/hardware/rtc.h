#ifndef __SAVM_HARDWARE_RTC_H_
#define __SAVM_HARDWARE_RTC_H_ 1

#include <savm/error.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

struct savm;

typedef struct {
	int since;
	int end;
	int finished;
} savm_rtc_timer_t;

typedef struct {
	size_t timerCount;
	savm_rtc_timer_t* timers;
} savm_rtc_t;

savm_error_e savm_rtc_create(savm_rtc_t* rtc);
savm_error_e savm_rtc_destroy(savm_rtc_t* rtc);
savm_error_e savm_rtc_reset(savm_rtc_t* rtc,struct savm* vm);
savm_error_e savm_rtc_cycle(savm_rtc_t* rtc,struct savm* vm);

#endif
