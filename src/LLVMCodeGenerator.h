#pragma once

#include "AST.h"

#include <unordered_map>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Analysis/Verifier.h>

class LLVMCodeGenerator : public ASTNodeVisitor {
	public:
		LLVMCodeGenerator();
		virtual ~LLVMCodeGenerator();
	
		bool build_ir(ASTNode* ast);
		bool write_ll_file(const char* path);
		bool write_executable(const char* path);
	
		virtual const void* visit(ASTNode* node);
		virtual const void* visit(ASTNop* node);
		virtual const void* visit(ASTExpression* node);
		virtual const void* visit(ASTSequence* node);
		virtual const void* visit(ASTVariableRef* node);
		virtual const void* visit(ASTVariableDec* node);
		virtual const void* visit(ASTFunctionRef* node);
		virtual const void* visit(ASTFunctionProto* node);
		virtual const void* visit(ASTFunctionDef* node);
		virtual const void* visit(ASTStructMemberRef* node);
		virtual const void* visit(ASTFloatingPoint* node);
		virtual const void* visit(ASTInteger* node);
		virtual const void* visit(ASTConstantArray* node);
		virtual const void* visit(ASTUnaryOp* node);
		virtual const void* visit(ASTBinaryOp* node);
		virtual const void* visit(ASTReturn* node);
		virtual const void* visit(ASTInlineAsm* node);
		virtual const void* visit(ASTFunctionCall* node);
		virtual const void* visit(ASTCondition* node);
		virtual const void* visit(ASTWhileLoop* node);
			
	private:
		llvm::Value* _lvalue(ASTExpression* exp);
		llvm::Value* _rvalue(ASTExpression* exp, C3TypePtr type = nullptr);
		llvm::Type* _llvm_type(C3TypePtr type);
	
		llvm::LLVMContext& _context;
		llvm::Module* _module;
		llvm::IRBuilder<> _builder;
		llvm::Function* _current_function;
		std::unordered_map<std::string, llvm::Value*> _named_values;
		std::unordered_map<std::string, llvm::Type*> _named_types;
};
