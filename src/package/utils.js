const fs = require("fs");
const jints = require("jints");

function writeArray(file,array) {
	var fd = fs.openSync(file,"w");
	for(var i = 0;i < array.length;i++) {
		var value = Buffer.from(new jints.UInt64(array[i]).toArray());
		fs.writeSync(fd,value);
	}
	fs.closeSync(fd);
}

module.exports = {
	writeArray: writeArray
};
