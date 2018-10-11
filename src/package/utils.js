const jints = require("jints");

function decode64bits(buff,size) {
	if(typeof(size) != "number") size = 1;
	return new jints.UInt64(buff).toArray();
	//return _.float64("value").unpack(buff);
}

function encode64bits(i,size) {
	if(typeof(size) != "number") size = 1;
	return new jints.UInt64(i).toArray();
	//return _.float64("value",size).pack(i);
}

module.exports = {
	decode64bits: decode64bits,
	encode64bits: encode64bits
};
