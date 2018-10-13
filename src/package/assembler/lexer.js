const chevrotain = require("chevrotain");
const createToken = chevrotain.createToken;
const Lexer = chevrotain.Lexer;

const TOKEN_BASE = {
	"address": createToken({ name: "address", pattern: /\$0x[0-9A-F]+/ }),
	"char": createToken({ name: "char", pattern: /'(\\?[^'\n]|\\')'?/ }),
	"comma": createToken({ name: "comma", pattern: /,/ }),
	"comment": createToken({ name: "comment", pattern: /#.*/, group: chevrotain.Lexer.SKIPPED }),
	"fn": createToken({ name: "fn", pattern: /[a-zA-Z0-9]+:/ }),
	"identifier": createToken({ name: "identifier", pattern: /\w+/ }),
	"integer": createToken({ name: "integer", pattern: /0|[1-9]\d+/ }),
	"register": createToken({ name: "register", pattern: /%[a-zA-Z0-9]+/ }),
	"whitespace": createToken({ name: "whitespace", pattern: /\s+/, group: chevrotain.Lexer.SKIPPED })
};

const all_tokens = (() => {
	var tokens = [];
	for(var token in TOKEN_BASE) tokens.push(TOKEN_BASE[token]);
	return tokens;
})();

module.exports = new Lexer(all_tokens);
module.exports.all_tokens = all_tokens;
module.exports.TOKEN_BASE = TOKEN_BASE;
