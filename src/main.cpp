#include <stdio.h>

#include "Preprocessor.h"
#include "Parser.h"
#include "LLVMCodeGenerator.h"

int main(int argc, char* argv[]) {
	if (argc < 2) {
		printf("Usage: %s in [out]\n", argv[0]);
		return 1;
	}
	
	// PREPROCESS

	Preprocessor pp;

	if (!pp.process_file(argv[1])) {
		printf("Couldn't preprocess file.\n");
		return 1;
	}

	// PARSE

	Parser p;
	
	ASTSequence* ast = p.generate_ast(pp.tokens());
	
	if (p.errors().size() > 0) {
		delete ast;
		for (const ParseError& e : p.errors()) {
			printf("Error: %s\n", e.message.c_str());
			e.token->print_pointer();
		}
		return 1;
	}
	
	if (!ast) {
		printf("Couldn't generate AST.\n");
		return 1;
	}

	ast->print();

	// GENERATE CODE

	LLVMCodeGenerator cg;
	
	if (!cg.build_ir(ast)) {
		printf("Couldn't build IR.\n");
		delete ast;
		return 1;
	}
	
	if (argc >= 3) {
		cg.write_executable(argv[2]);
	}
	
	delete ast;

	return 0;
}
