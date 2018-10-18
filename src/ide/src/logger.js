const path = require("path");
const paths = require("./paths");
const winston = require("winston");

console.log(winston.format);

module.exports = winston.createLogger({
	level: require("./settings").read()["loggerLevel"],
	format: winston.format.combine(
		winston.format.timestamp(),
		winston.format.prettyPrint(),
		winston.format.splat()
	),
	transports: [
		new winston.transports.File({ filename: path.join(paths["LOGS"],"error.log"), level: "error" }),
		new winston.transports.File({ filename: path.join(paths["LOGS"],"all.log") }),
		new winston.transports.Console()
	]
});
