const { Parser } = require("chevrotain");
const Lexer = require("./lexer.js");

class SAParser extends Parser {
	constructor(config) {
		super(Lexer.all_tokens,config);
		
		const $ = this;
		
		$.RULE("instr",() => {
			$.CONSUME(Lexer.TOKEN_BASE.identifier);
			$.OR([
				{ ALT: () => $.CONSUME2(Lexer.TOKEN_BASE.address) },
				{ ALT: () => $.CONSUME3(Lexer.TOKEN_BASE.identifier) },
				{ ALT: () => $.CONSUME4(Lexer.TOKEN_BASE.register) },
				{ ALT: () => {
					$.CONSUME5(Lexer.TOKEN_BASE.address);
					$.CONSUME6(Lexer.TOKEN_BASE.comma);
					$.CONSUME7(Lexer.TOKEN_BASE.address);
				} },
				{ ALT: () => {
					$.CONSUME8(Lexer.TOKEN_BASE.address);
					$.CONSUME9(Lexer.TOKEN_BASE.comma);
					$.CONSUME10(Lexer.TOKEN_BASE.char);
				} },
				{ ALT: () => {
					$.CONSUME11(Lexer.TOKEN_BASE.address);
					$.CONSUME12(Lexer.TOKEN_BASE.comma);
					$.CONSUME13(Lexer.TOKEN_BASE.register);
				} },
				{ ALT: () => {
					$.CONSUME14(Lexer.TOKEN_BASE.register);
					$.CONSUME15(Lexer.TOKEN_BASE.comma);
					$.CONSUME16(Lexer.TOKEN_BASE.address);
				} },
				{ ALT: () => {
					$.CONSUME17(Lexer.TOKEN_BASE.register);
					$.CONSUME18(Lexer.TOKEN_BASE.comma);
					$.CONSUME19(Lexer.TOKEN_BASE.char);
				} },
				{ ALT: () => {
					$.CONSUME20(Lexer.TOKEN_BASE.register);
					$.CONSUME21(Lexer.TOKEN_BASE.comma);
					$.CONSUME22(Lexer.TOKEN_BASE.register);
				} }
			]);
		});
		
		/* MISC */
		$.RULE("functionStatement",() => {
			$.CONSUME(Lexer.TOKEN_BASE.fn);
			$.SUBRULE($.instr);
		});
		
		this.performSelfAnalysis();
	}
}

module.exports = {
	Parser: SAParser,
	parseInput: (text) => {
		const lexingResult = Lexer.tokenize(text);
		const parser = new SAParser({
			maxLookahead: 10
		});
		console.log(lexingResult.tokens);
		parser.input = lexingResult.tokens;
		parser.functionStatement();
		return parser;
	}
};
