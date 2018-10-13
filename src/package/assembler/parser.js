const { Parser } = require("chevrotain");
const Lexer = require("./lexer.js");

class SAParser extends Parser {
	constructor(config) {
		super(Lexer.all_tokens,config);
		
		this.maxLookahead *= 2;
		
		const $ = this;
		
		$.RULE("reg2RegInstr",() => {
			$.CONSUME(Lexer.TOKEN_BASE.register);
			$.CONSUME2(Lexer.TOKEN_BASE.comma);
			$.CONSUME3(Lexer.TOKEN_BASE.register);
		});
		
		$.RULE("int2RegInstr",() => {
			$.CONSUME(Lexer.TOKEN_BASE.register);
			$.CONSUME2(Lexer.TOKEN_BASE.comma);
			$.CONSUME3(Lexer.TOKEN_BASE.integer);
		});
		
		$.RULE("mem2RegInstr",() => {
			$.CONSUME(Lexer.TOKEN_BASE.register);
			$.CONSUME2(Lexer.TOKEN_BASE.comma);
			$.CONSUME3(Lexer.TOKEN_BASE.address);
		});
		
		$.RULE("mem2MemInstr",() => {
			$.CONSUME(Lexer.TOKEN_BASE.address);
			$.CONSUME2(Lexer.TOKEN_BASE.comma);
			$.CONSUME3(Lexer.TOKEN_BASE.address);
		});
		
		$.RULE("int2MemInstr",() => {
			$.CONSUME(Lexer.TOKEN_BASE.address);
			$.CONSUME2(Lexer.TOKEN_BASE.comma);
			$.CONSUME3(Lexer.TOKEN_BASE.integer);
		});
		
		$.RULE("reg2MemInstr",() => {
			$.CONSUME(Lexer.TOKEN_BASE.register);
			$.CONSUME2(Lexer.TOKEN_BASE.comma);
			$.CONSUME3(Lexer.TOKEN_BASE.address);
		});
		
		/* instructions */
		$.RULE("instrADD",() => {
			$.CONSUME(Lexer.TOKEN_INSTRUCTIONS.add);
			$.OR([
				{ ALT: () => $.SUBRULE($.reg2RegInstr) },
				{ ALT: () => $.SUBRULE($.mem2MemInstr) }
			]);
		});
		
		$.RULE("instrSUB",() => {
			$.CONSUME(Lexer.TOKEN_INSTRUCTIONS.sub);
			$.OR([
				{ ALT: () => $.SUBRULE($.reg2RegInstr) },
				{ ALT: () => $.SUBRULE($.mem2MemInstr) }
			]);
		});
		
		$.RULE("instrMUL",() => {
			$.CONSUME(Lexer.TOKEN_INSTRUCTIONS.mul);
			$.OR([
				{ ALT: () => $.SUBRULE($.reg2RegInstr) },
				{ ALT: () => $.SUBRULE($.mem2MemInstr) }
			]);
		});
		
		$.RULE("instrDIV",() => {
			$.CONSUME(Lexer.TOKEN_INSTRUCTIONS.div);
			$.OR([
				{ ALT: () => $.SUBRULE($.reg2RegInstr) },
				{ ALT: () => $.SUBRULE($.mem2MemInstr) }
			]);
		});
		
		$.RULE("instrAND",() => {
			$.CONSUME(Lexer.TOKEN_INSTRUCTIONS.and);
			$.OR([
				{ ALT: () => $.SUBRULE($.reg2RegInstr) },
				{ ALT: () => $.SUBRULE($.mem2MemInstr) }
			]);
		});
		
		$.RULE("instrOR",() => {
			$.CONSUME(Lexer.TOKEN_INSTRUCTIONS.or);
			$.OR([
				{ ALT: () => $.SUBRULE($.reg2RegInstr) },
				{ ALT: () => $.SUBRULE($.mem2MemInstr) }
			]);
		});
		
		/* MISC */
		$.RULE("functionStatement",() => {
			$.CONSUME(Lexer.TOKEN_BASE.fn);
			$.OR([
				{ ALT: () => $.SUBRULE($.instrADD) },
				{ ALT: () => $.SUBRULE($.instrSUB) },
				{ ALT: () => $.SUBRULE($.instrMUL) },
				{ ALT: () => $.SUBRULE($.instrDIV) },
				{ ALT: () => $.SUBRULE($.instrAND) },
				{ ALT: () => $.SUBRULE($.instrOR) },
				{ ALT: () => $.CONSUME($.tokensMap.EOF) }
			]);
		});
		
		this.performSelfAnalysis();
	}
}
module.exports = {
	Parser: SAParser,
	parseInput: (text) => {
		const lexingResult = Lexer.tokenize(text);
		const parser = new SAParser([]);
		parser.input = lexingResult.tokens;
		parser.functionStatement();
		return parser;
	}
};
