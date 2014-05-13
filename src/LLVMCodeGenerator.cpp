#include "LLVMCodeGenerator.h"

#include <llvm/IR/InlineAsm.h>
#include <llvm/PassManager.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Assembly/PrintModulePass.h>

LLVMCodeGenerator::LLVMCodeGenerator() 
	: _context(llvm::getGlobalContext())
	, _module(new llvm::Module("top", _context))
	, _builder(llvm::getGlobalContext())
	, _current_function(nullptr)
{
}

LLVMCodeGenerator::~LLVMCodeGenerator() {
	delete _module;
	// TODO: i'm sure this leaks all over the place...
}

bool LLVMCodeGenerator::build_ir(ASTNode* ast) {
	llvm::FunctionType* funcType = llvm::FunctionType::get(_builder.getVoidTy(), false);
	llvm::Function* mainFunc = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "main", _module);
	llvm::BasicBlock* entry = llvm::BasicBlock::Create(_context, "entry", mainFunc);
	_builder.SetInsertPoint(entry);

	_current_function = mainFunc;
	ast->accept(this);

	_builder.CreateRetVoid();

	_module->dump();

	return !llvm::verifyModule(*_module, llvm::PrintMessageAction); // verifyModule returns false on success
}

bool LLVMCodeGenerator::write_ll_file(const char* path) {
	std::string error;
	llvm::tool_output_file fdout(path, error, llvm::sys::fs::F_None);

	if (!error.empty()) {
		printf("%s\n", error.c_str());
		return false;
	}

	llvm::PassManager pm;
	pm.add(llvm::createPrintModulePass(&(fdout.os())));
	pm.run(*_module);
	
	fdout.keep();

	return true;
}

bool LLVMCodeGenerator::write_executable(const char* path) {
	char tmp[L_tmpnam];
	
	// TODO: don't use tmpnam
	if (!tmpnam(tmp) || !write_ll_file(tmp)) {
		return false;
	}

	std::string cmd = "llc \"";
	cmd += tmp;
	cmd += "\" -o - | clang -x assembler -nostdlib -lSystem - -o \"";
	cmd += path;
	cmd += '"';

	printf("%s\n", cmd.c_str());

	int ret = system(cmd.c_str());

	remove(tmp);

	return (ret == 0);
}

const void* LLVMCodeGenerator::visit(ASTNode* node) {
	assert(false);
	return nullptr;
}

const void* LLVMCodeGenerator::visit(ASTNop* node) {
	return nullptr;
}

const void* LLVMCodeGenerator::visit(ASTExpression* node) {
	assert(false);
	return nullptr;
}

const void* LLVMCodeGenerator::visit(ASTSequence* node) {
	// TODO: wrap in a block?
	for (ASTNode* n : node->sequence) {
		n->accept(this);
	}
	return nullptr;
}

const void* LLVMCodeGenerator::visit(ASTVariableRef* node) {
	llvm::Value* v = _named_values[node->var->global_name()];
	assert(v);
	return v;
}

const void* LLVMCodeGenerator::visit(ASTVariableDec* node) {
	llvm::AllocaInst* alloca = _builder.CreateAlloca(_llvm_type(node->var->type()), 0, node->var->global_name().c_str());
	_named_values[node->var->global_name()] = alloca;

	if (node->init) {
		_builder.CreateStore(_rvalue(node->init, node->var->type()), alloca);
	}

	return nullptr;
}

const void* LLVMCodeGenerator::visit(ASTFunctionRef* node) {
	llvm::Value* v = _module->getFunction(node->func->global_name());
	assert(v);
	return v;
}

const void* LLVMCodeGenerator::visit(ASTFunctionProto* node) {
	std::vector<llvm::Type*> arg_types;
	for (C3TypePtr type : node->func->arg_types()) {
		arg_types.push_back(_llvm_type(type));
	}

	const std::string& name = node->func->global_name();

	llvm::FunctionType* ft = llvm::FunctionType::get(_llvm_type(node->func->return_type()), arg_types, false);
	llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name, _module);

	if (f->getName() != name) {
		// function already exists. erase this, get the old one
		f->eraseFromParent();
		f = _module->getFunction(name);
	}

	return f;
}

const void* LLVMCodeGenerator::visit(ASTFunctionDef* node) {
	llvm::Function* f = (llvm::Function*)node->proto->accept(this);
	
	// set names
	size_t i = 0;
	for (llvm::Function::arg_iterator ai = f->arg_begin(); ai != f->arg_end(); ++ai) {
		ai->setName(node->proto->arg_names[i]);
		++i;
	}

	// create the block
	llvm::BasicBlock* entry = llvm::BasicBlock::Create(_context, "entry", f);
	llvm::IRBuilderBase::InsertPoint ip = _builder.saveIP();
	_builder.SetInsertPoint(entry);

	// load arguments
	// we copy them to allow them to be lvalues
	// TODO: make sure llvm can optimize the unnecessary copies out?
	i = 0;
	for (llvm::Function::arg_iterator ai = f->arg_begin(); ai != f->arg_end(); ++ai) {
		llvm::AllocaInst* alloca = _builder.CreateAlloca(ai->getType(), 0, node->arg_prefix + node->proto->arg_names[i]);
		_named_values[node->arg_prefix + node->proto->arg_names[i]] = alloca;
		_builder.CreateStore(ai, alloca);
		++i;
	}

	// build the body
	auto prev_function = _current_function;
	_current_function = f;
	node->body->accept(this);
	_current_function = prev_function;

	// end the block
	_builder.restoreIP(ip);
	
	return nullptr;
}

const void* LLVMCodeGenerator::visit(ASTStructMemberRef* node) {
	return _builder.CreateStructGEP((llvm::Value*)node->structure->accept(this), node->index);
}

const void* LLVMCodeGenerator::visit(ASTFloatingPoint* node) {
	return llvm::ConstantFP::get(_context, llvm::APFloat(node->value));
}

const void* LLVMCodeGenerator::visit(ASTInteger* node) {
	return llvm::ConstantInt::get(_context, llvm::APInt(64, node->value));
}

const void* LLVMCodeGenerator::visit(ASTConstantArray* node) {
	llvm::Constant* v = nullptr;

	assert(node->type->points_to());
	switch (node->type->points_to()->type()) {
		case C3TypeTypeBuiltInInt8:
			v = llvm::ConstantDataArray::get(_context, llvm::ArrayRef<uint8_t>((uint8_t*)node->data, node->size));
			break;
		default:
			break;
	}
	assert(v);

	llvm::GlobalVariable* gv = new llvm::GlobalVariable(*_module, v->getType(), true, llvm::GlobalValue::PrivateLinkage, v, "", 0, llvm::GlobalVariable::NotThreadLocal);
	gv->setUnnamedAddr(true);
	llvm::Value* zero   = llvm::ConstantInt::get(llvm::Type::getInt32Ty(_context), 0);
	llvm::Value* args[] = { zero, zero };
	return _builder.CreateInBoundsGEP(gv, args);
}

const void* LLVMCodeGenerator::visit(ASTUnaryOp* node) {
	if (node->op == "&") {
		assert(node->right->is_lvalue && !node->is_lvalue);
		// we don't really have to do anything
		// the right side was an lvalue (a reference) and the result of this is that same reference, marked as an rvalue
		return node->right->accept(this);
	} else if (node->op == "*") {
		assert(node->is_lvalue);
		return _rvalue(node->right);
	}
	// TODO: implement other ops
	assert(false);
	return nullptr;
}

const void* LLVMCodeGenerator::visit(ASTBinaryOp* node) {
	if (node->op == "=") {
		// assign
		llvm::Value* left = _lvalue(node->left);
		_builder.CreateStore(_rvalue(node->right, node->left->type), left);
		return left;
	} else {
		auto left       = _rvalue(node->left, node->type);
		auto right      = _rvalue(node->right, node->type);
		bool signed_op  = node->left->type->is_signed() || node->right->type->is_signed();
		if (node->op == "*") {
			return left->getType()->isFPOrFPVectorTy() ? _builder.CreateFMul(left, right) : _builder.CreateMul(left, right);
		} else if (node->op == "/") {
			return left->getType()->isFPOrFPVectorTy() ? _builder.CreateFDiv(left, right) : (signed_op ? _builder.CreateSDiv(left, right) : _builder.CreateUDiv(left, right));
		} else if (node->op == "+") {
			return left->getType()->isFPOrFPVectorTy() ? _builder.CreateFAdd(left, right) : _builder.CreateAdd(left, right);
		} else if (node->op == "-") {
			return left->getType()->isFPOrFPVectorTy() ? _builder.CreateFSub(left, right) : _builder.CreateSub(left, right);
		} else if (node->op == "==") {
			return left->getType()->isFPOrFPVectorTy() ? _builder.CreateFCmpOEQ(left, right) : _builder.CreateICmpEQ(left, right);
		} else if (node->op == "!=") {
			return left->getType()->isFPOrFPVectorTy() ? _builder.CreateFCmpONE(left, right) : _builder.CreateICmpNE(left, right);
		} else if (node->op == "<") {
			return left->getType()->isFPOrFPVectorTy() ? _builder.CreateFCmpOLT(left, right) : (signed_op ? _builder.CreateICmpSLT(left, right) : _builder.CreateICmpULT(left, right));
		} else if (node->op == "<=") {
			return left->getType()->isFPOrFPVectorTy() ? _builder.CreateFCmpOLE(left, right) : (signed_op ? _builder.CreateICmpSLE(left, right) : _builder.CreateICmpULE(left, right));
		} else if (node->op == ">") {
			return left->getType()->isFPOrFPVectorTy() ? _builder.CreateFCmpOGT(left, right) : (signed_op ? _builder.CreateICmpSGT(left, right) : _builder.CreateICmpUGT(left, right));
		} else if (node->op == ">=") {
			return left->getType()->isFPOrFPVectorTy() ? _builder.CreateFCmpOGE(left, right) : (signed_op ? _builder.CreateICmpSGE(left, right) : _builder.CreateICmpUGE(left, right));
		}
	}

	assert(false);
	return nullptr;
}

const void* LLVMCodeGenerator::visit(ASTReturn* node) {
	if (!node->value) {
		_builder.CreateRetVoid();
	} else {
		_builder.CreateRet(_rvalue(node->value));
	}
	return nullptr;
}

const void* LLVMCodeGenerator::visit(ASTInlineAsm* node) {
	std::vector<llvm::Type*> input_types;
	std::vector<llvm::Value*> args;

	size_t i = 0;
	for (ASTExpression* exp : node->inputs) {
		if (node->constraints[node->outputs.size() + i].find('*') != std::string::npos) {
			// indirect
			args.push_back(_lvalue(exp));
			input_types.push_back(_llvm_type(exp->type)->getPointerTo());
		} else {
			args.push_back(_rvalue(exp));
			input_types.push_back(_llvm_type(exp->type));
		}
		++i;
	}

	std::vector<llvm::Type*> output_types;
	for (ASTExpression* exp : node->outputs) {
		output_types.push_back(_llvm_type(exp->type));
	}

	llvm::Type* type = _builder.getVoidTy();
	
	if (output_types.size() == 1) {
		type = output_types[0];
	} else if (output_types.size() > 1) {
		type = llvm::StructType::create(output_types);
	}

	std::string constraint_str = "";
	bool first = true;
	for (std::string& c : node->constraints) {
		if (first) {
			first = false;
		} else {
			constraint_str += ',';
		}
		constraint_str += c;
	}

	llvm::InlineAsm* as = llvm::InlineAsm::get(llvm::FunctionType::get(type, input_types, false), node->assembly, constraint_str, true);
	_builder.Insert(llvm::CallInst::Create(as, args));
	
	// TODO: store outputs
	
	return nullptr;
}

const void* LLVMCodeGenerator::visit(ASTFunctionCall* node) {
	std::vector<llvm::Value*> args;
	for (ASTExpression* exp : node->args) {
		args.push_back(_rvalue(exp));
	}
	return _builder.CreateCall(_rvalue(node->func), args);
}

const void* LLVMCodeGenerator::visit(ASTCondition* node) {
	assert(_current_function);
	
	llvm::BasicBlock* returnBlock = llvm::BasicBlock::Create(_context, "post", _current_function);

	// create the true block
	llvm::BasicBlock* trueBlock = llvm::BasicBlock::Create(_context, "true", _current_function);
	llvm::IRBuilderBase::InsertPoint ip = _builder.saveIP();
	_builder.SetInsertPoint(trueBlock);
	node->truePath->accept(this);
	_builder.CreateBr(returnBlock);
	_builder.restoreIP(ip);

	// create the false block
	llvm::BasicBlock* falseBlock = llvm::BasicBlock::Create(_context, "false", _current_function);
	ip = _builder.saveIP();
	_builder.SetInsertPoint(falseBlock);
	node->falsePath->accept(this);
	_builder.CreateBr(returnBlock);
	_builder.restoreIP(ip);

	_builder.CreateCondBr(_rvalue(node->condition), trueBlock, falseBlock);
	_builder.SetInsertPoint(returnBlock);

	return nullptr;
}

const void* LLVMCodeGenerator::visit(ASTWhileLoop* node) {
	assert(_current_function);
	
	llvm::BasicBlock* whileBlock = llvm::BasicBlock::Create(_context, "while", _current_function);
	llvm::BasicBlock* breakBlock = llvm::BasicBlock::Create(_context, "break", _current_function);

	_builder.CreateBr(whileBlock);
	_builder.SetInsertPoint(whileBlock);

	// create the loop block
	llvm::BasicBlock* bodyBlock = llvm::BasicBlock::Create(_context, "body", _current_function);
	llvm::IRBuilderBase::InsertPoint ip = _builder.saveIP();
	_builder.SetInsertPoint(bodyBlock);
	node->body->accept(this);
	_builder.CreateBr(whileBlock);
	_builder.restoreIP(ip);

	_builder.CreateCondBr(_rvalue(node->condition), bodyBlock, breakBlock);
	_builder.SetInsertPoint(breakBlock);

	return nullptr;
}

llvm::Value* LLVMCodeGenerator::_lvalue(ASTExpression* exp) {
	assert(exp->is_lvalue);
	return (llvm::Value*)exp->accept(this);
}

llvm::Value* LLVMCodeGenerator::_rvalue(ASTExpression* exp, C3TypePtr type) {
	llvm::Value* v = (llvm::Value*)exp->accept(this);
	if (exp->is_lvalue) {
		v = _builder.CreateLoad(v);
	}
	if (type && *type != *exp->type) {
		if (exp->type->is_integer()) {
			// integer promotion / conversion
			assert(type->is_integer());
			v = _builder.CreateIntCast(v, _llvm_type(type), exp->type->is_signed());
		} else {
			assert(false);
		}
	}
	return v;
}

llvm::Type* LLVMCodeGenerator::_llvm_type(C3TypePtr type) {
	switch (type->type()) {
		case C3TypeTypePointer:
			return _llvm_type(type->points_to())->getPointerTo();
		case C3TypeTypeBuiltInVoid:
			return llvm::Type::getVoidTy(_context);
		case C3TypeTypeBuiltInBool:
		case C3TypeTypeBuiltInInt8:
			return llvm::Type::getInt8Ty(_context);
		case C3TypeTypeBuiltInInt32:
			return llvm::Type::getInt32Ty(_context);
		case C3TypeTypeBuiltInInt64:
			return llvm::Type::getInt64Ty(_context);
		case C3TypeTypeBuiltInDouble:
			return llvm::Type::getDoubleTy(_context);
		case C3TypeTypeFunction:
			assert(false); // TODO: ???
		case C3TypeTypeStruct: {
			llvm::StructType* ret = nullptr;
			
			if (!_named_types.count(type->global_name())) {
				_named_types[type->global_name()] = ret = llvm::StructType::create(_context, type->global_name());
			} else {
				auto t = _named_types[type->global_name()];
				assert(t->isStructTy());
				ret = (llvm::StructType*)t;
			}
			
			if (ret->isOpaque() && type->is_defined()) {
				std::vector<llvm::Type*> elements;
				auto& member_vars = type->struct_definition().member_vars();
				for (auto& var : member_vars) {
					elements.push_back(_llvm_type(var.type));
				}
				ret->setBody(elements, true);
			}
			
			return ret;
		}
	}
	
	assert(false);
	return llvm::Type::getVoidTy(_context);
}