const RTC_BASE = 0x1000000A;
const RTC_SIZE = 0x00000007;
const RTC_END = RTC_BASE+RTC_SIZE;

Date.prototype.stdTimezoneOffset = function () {
	var jan = new Date(this.getFullYear(),0,1);
	var jul = new Date(this.getFullYear(),6,1);
	return Math.max(jan.getTimezoneOffset(),jul.getTimezoneOffset());
}

Date.prototype.isDstObserved = function () {
	return this.getTimezoneOffset() < this.stdTimezoneOffset();
}

class RTC {
	constructor() {
		this.timers = [];
	}
	destroy() {}
	reset(vm) {
		vm.mmap(RTC_BASE,RTC_END,i => {
			switch(i) {
				case 0: return new Date().getSeconds();
				case 1: return new Date().getMinutes();
				case 2: return new Date().getHours();
				case 3: return new Date().getDate();
				case 4: return new Date().getMonth();
				case 5: return new Date().getFullYear();
				case 6: return new Date().isDstObserved();
				
				case 7: return this.timers.length;
			}
			return 0;
		},(i,v) => {
			switch(i) {
				case 7:
					this.createTimer(v,vm);
					break;
			}
		});
	}
	cycle(vm) {}
	createTimer(time,vm) {
		this.timers.push(setInterval((i) => {
			vm.cpu.cores[vm.cpu.currentCore].regs.data[0] = i;
			vm.intr(6);
			clearInterval(this.timers[i]);
			delete this.timers[i];
		},time,this.timers.length));
	}
}
module.exports = RTC;
