module.exports = {
	Assembler: require("./assembler"),
	VirtualMachine: require("./vm")
};

if(typeof(window) != "undefined") {
	if(typeof(window.SherwoodArch) == "undefined") window.SherwoodArch = module.exports;
}
