const BlockDevice = require("blockdevice");
const CHS = require("chs");
const fs = require("fs");

const FLAGS = {
	"ENABLE": (1 << 0),
	"RO": (1 << 1)
};

const STATUS = {
	"BUSY": (1 << 0),
	"WRITING": (1 << 1),
	"READING": (1 << 2)
};

const ERROR = {
	"NONE": 0,
	"READ": 1,
	"WRITE": 2
};

class HDDStorageDevice {
	constructor(drive) {
		this.hdr = {
			vendorID: 0xA900,
			deviceID: 0x0000,
			rev: 0,
			type: 1,
			classCode: 0,
			subclass: 0
		};
		
		this.name = "Basic HDD";
		
		this.blkdev = new BlockDevice({
			path: drive,
			mode: "r+",
			headsPerTrack: 255,
			sectorsPerTrack: 255
		});
		
		this.status = 0;
		this.error = 0;
	}
	create(dev,vm) {
		this.blkdev.open(err => {
			if(err) throw err;
			
			this.lbaStart = this.blkdev.getLBA(0,0,0);
			this.lbaEnd = this.blkdev.getLBA(0,0,1);
			this.flags = 0;
		});
	}
	destroy(dev) {
		this.blkdev.close(err => {
			if(err) throw err;
		});
	}
	read(dev,i,vm) {
		if(!(this.flags & FLAGS["ENABLE"])) return 0;
		switch(i) {
			case 0: return this.flags;
			case 1: return this.blkdev.blockSize;
			case 2: return this.blkdev.headsPerTrack;
			case 3: return this.blkdev.sectorsPerTrack;
			case 4: return this.lbaStart;
			case 5: return this.lbaEnd;
			case 6: return this.status;
			case 7:
				var e = this.error;
				this.error = ERROR["NONE"];
				return e;
		}
	}
	write(dev,i,v,vm) {
		switch(i) {
			case 0:
				this.flags = v;
				this.error = ERROR["NONE"];
				break;
			case 1:
				if(!(this.flags & FLAGS["ENABLE"])) return;
				if(!(this.status & STATUS["BUSY"])) {
					this.status |= STATUS["BUSY"];
					this.lbaStart = CHS.fromLBA(this.lbaStart,this.blkdev.headsPerTrack,this.blkdev.sectorsPerTrack);
					this.lbaStart.cylinder = v;
					this.lbaStart = this.lbaStart.getLBA(this.blkdev.headsPerTrack,this.blkdev.sectorsPerTrack);
					this.status &= ~STATUS["BUSY"];
				}
				break;
			case 2:
				if(!(this.flags & FLAGS["ENABLE"])) return;
				if(!(this.status & STATUS["BUSY"])) {
					this.status |= STATUS["BUSY"];
					this.lbaStart = CHS.fromLBA(this.lbaStart,this.blkdev.headsPerTrack,this.blkdev.sectorsPerTrack);
					this.lbaStart.head = v;
					this.lbaStart = this.lbaStart.getLBA(this.blkdev.headsPerTrack,this.blkdev.sectorsPerTrack);
					this.status &= ~STATUS["BUSY"];
				}
				break;
			case 3:
				if(!(this.flags & FLAGS["ENABLE"])) return;
				if(!(this.status & STATUS["BUSY"])) {
					this.status |= STATUS["BUSY"];
					this.lbaStart = CHS.fromLBA(this.lbaStart,this.blkdev.headsPerTrack,this.blkdev.sectorsPerTrack);
					this.lbaStart.sector = v;
					this.lbaStart = this.lbaStart.getLBA(this.blkdev.headsPerTrack,this.blkdev.sectorsPerTrack);
					this.status &= ~STATUS["BUSY"];
				}
				break;
			case 4:
				if(!(this.flags & FLAGS["ENABLE"])) return;
				if(!(this.status & STATUS["BUSY"])) {
					this.status |= STATUS["BUSY"];
					this.lbaEnd = CHS.fromLBA(this.lbaEnd,this.blkdev.headsPerTrack,this.blkdev.sectorsPerTrack);
					this.lbaEnd.cylinder = v;
					this.lbaEnd = this.lbaStart.getLBA(this.blkdev.headsPerTrack,this.blkdev.sectorsPerTrack);
					this.status &= ~STATUS["BUSY"];
				}
				break;
			case 5:
				if(!(this.flags & FLAGS["ENABLE"])) return;
				if(!(this.status & STATUS["BUSY"])) {
					this.status |= STATUS["BUSY"];
					this.lbaEnd = CHS.fromLBA(this.lbaEnd,this.blkdev.headsPerTrack,this.blkdev.sectorsPerTrack);
					this.lbaEnd.head = v;
					this.lbaEnd = this.lbaStart.getLBA(this.blkdev.headsPerTrack,this.blkdev.sectorsPerTrack);
					this.status &= ~STATUS["BUSY"];
				}
				break;
			case 6:
				if(!(this.flags & FLAGS["ENABLE"])) return;
				if(!(this.status & STATUS["BUSY"])) {
					this.status |= STATUS["BUSY"];
					this.lbaEnd = CHS.fromLBA(this.lbaEnd,this.blkdev.headsPerTrack,this.blkdev.sectorsPerTrack);
					this.lbaEnd.sector = v;
					this.lbaEnd = this.lbaStart.getLBA(this.blkdev.headsPerTrack,this.blkdev.sectorsPerTrack);
					this.status &= ~STATUS["BUSY"];
				}
				break;
			case 7: /* WRITE */
				if(!(this.flags & FLAGS["ENABLE"])) return;
				if(!(this.flags & FLAGS["RO"]) && !this.isWriting && !(this.status & STATUS["BUSY"]) && !(this.status & STATUS["WRITING"])) {
					this.status |= STATUS["BUSY"];
					this.status |= STATUS["WRITING"];
					var buff = Buffer.from(this.blkdev.blockSize);
					for(var x = 0;x < buff.length;x++) buff[x] = vm.read(v+x);
					this.blkdev.writeBlocks(this.lbaStart,buff,err => {
						if(err) {
							console.error(err);
							this.error = ERROR["WRITE"];
						}
						this.status &= ~STATUS["BUSY"];
						this.status &= ~STATUS["WRITING"];
					});
				}
				break;
			case 8: /* READ */
				if(!(this.flags & FLAGS["ENABLE"])) return;
				if(!(this.flags & FLAGS["RO"]) && !this.isWriting && !(this.status & STATUS["BUSY"]) && !(this.status & STATUS["READING"])) {
					this.status |= STATUS["BUSY"];
					this.status |= STATUS["READING"];
					this.blkdev.readBlocks(this.lbaStart,this.lbaEnd,(err,buff) => {
						if(err) {
							console.error(err);
							this.error = ERROR["READ"];
							this.status &= ~STATUS["BUSY"];
							this.status &= ~STATUS["READING"];
							return;
						}
						for(var x = 0;x < buff.length;x++) vm.write(v+x,buff[x]);
						this.status &= ~STATUS["BUSY"];
						this.status &= ~STATUS["READING"];
					});
				}
				break;
		}
	}
}
module.exports = HDDStorageDevice;
