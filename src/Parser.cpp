#include "Parser.h"
#include "Preprocessor.h"

#include <sstream>

Parser::Parser() {
	Scope global("");

	global.types["void"]   = C3Type::VoidType();
	global.types["bool"]   = C3Type::BoolType();
	global.types["char"]   = C3Type::CharType();
	global.types["int64"]  = C3Type::Int64Type();
	global.types["double"] = C3Type::DoubleType();

	_scopes.push_back(global);
	
	_keywords.insert("asm");
	_keywords.insert("return");

	// TODO: respect unary precedence
	_binary_ops["."]  = { 110, false };
	_binary_ops["->"] = { 110, false };
	 _unary_ops["+"]  = { 100, true };
	 _unary_ops["-"]  = { 100, true };
	 _unary_ops["*"]  = { 100, true };
	 _unary_ops["&"]  = { 100, true };
	_binary_ops["*"]  = {  80, false };
	_binary_ops["/"]  = {  80, false };
	_binary_ops["+"]  = {  60, false };
	_binary_ops["-"]  = {  60, false };
	_binary_ops["<"]  = {  50, false };
	_binary_ops["<="] = {  50, false };
	_binary_ops[">"]  = {  50, false };
	_binary_ops[">="] = {  50, false };
	_binary_ops["=="] = {  40, false };
	_binary_ops["!="] = {  40, false };
	_binary_ops["="]  = {  20, true };
}

ASTSequence* Parser::generate_ast(const std::list<TokenPtr>& tokens) {
	// may be called recursively when importing modules

	auto prev_cur_tok = _cur_tok;
	auto prev_end_tok = _end_tok;
	
	_cur_tok = tokens.begin();
	_end_tok = tokens.end();

	auto block = _parse_block();
	
	if (block && !_peek(ptt_end_token)) {
		_errors.push_back(ParseError("expected end of file", _token()));
		delete block;
		block = nullptr;
	}
	
	_cur_tok = prev_cur_tok;
	_end_tok = prev_end_tok;

	return block;
}

const std::list<ParseError>& Parser::errors() {
	return _errors;
}

TokenPtr Parser::_token() {
	if (_cur_tok != _end_tok) {
		return *_cur_tok;
	}

	class DummyToken : public Token {
		virtual TokenType type() { return TokenTypeOther; }
		virtual uint32_t flags() { return 0; }
		virtual const std::string value() { return std::string(); }
	};
	
	static TokenPtr dummy(new DummyToken());
	return dummy;
}

void Parser::_consume(size_t tokens) {
	for (size_t i = 0; i < tokens && _cur_tok != _end_tok; ++i) {
		++_cur_tok;
	}
}

TokenPtr Parser::_consume_token() {
	TokenPtr ret = _token();
	_consume(1);
	return ret;
}

bool Parser::_peek(ParserTokenType type) {
	TokenPtr tok = _token();
	switch (type) {
		case ptt_semicolon:
			return tok->type() == TokenTypePunctuator && tok->value() == ";";
		case ptt_colon:
			return tok->type() == TokenTypePunctuator && tok->value() == ":";
		case ptt_open_brace:
			return tok->type() == TokenTypePunctuator && tok->value() == "{";
		case ptt_close_brace:
			return tok->type() == TokenTypePunctuator && tok->value() == "}";
		case ptt_open_paren:
			return tok->type() == TokenTypePunctuator && tok->value() == "(";
		case ptt_close_paren:
			return tok->type() == TokenTypePunctuator && tok->value() == ")";
		case ptt_comma:
			return tok->type() == TokenTypePunctuator && tok->value() == ",";
		case ptt_asterisk:
			return tok->type() == TokenTypePunctuator && tok->value() == "*";
		case ptt_assignment:
			return tok->type() == TokenTypePunctuator && tok->value() == "=";
		case ptt_equality:
			return tok->type() == TokenTypePunctuator && tok->value() == "==";
		case ptt_inequality:
			return tok->type() == TokenTypePunctuator && tok->value() == "!=";
		case ptt_namespace_delimiter:
			return tok->type() == TokenTypePunctuator && tok->value() == "::";
		case ptt_keyword:
			return tok->type() == TokenTypeIdentifier && _keywords.count(tok->value()) > 0;
		case ptt_keyword_asm:
			return tok->type() == TokenTypeIdentifier && tok->value() == "asm";
		case ptt_keyword_return:
			return tok->type() == TokenTypeIdentifier && tok->value() == "return";
		case ptt_keyword_import:
			return tok->type() == TokenTypeIdentifier && tok->value() == "import";
		case ptt_keyword_if:
			return tok->type() == TokenTypeIdentifier && tok->value() == "if";
		case ptt_keyword_else:
			return tok->type() == TokenTypeIdentifier && tok->value() == "else";
		case ptt_keyword_while:
			return tok->type() == TokenTypeIdentifier && tok->value() == "while";
		case ptt_keyword_struct:
			return tok->type() == TokenTypeIdentifier && tok->value() == "struct";
		case ptt_keyword_namespace:
			return tok->type() == TokenTypeIdentifier && tok->value() == "namespace";
		case ptt_number:
			return tok->type() == TokenTypeNumber;
		case ptt_end_token:
			return _cur_tok == _end_tok;
		case ptt_unary_op:
			return tok->type() == TokenTypePunctuator && _unary_ops.find(tok->value()) != _unary_ops.end();
		case ptt_binary_op:
			return tok->type() == TokenTypePunctuator && _binary_ops.find(tok->value()) != _binary_ops.end();
		case ptt_string_literal:
			return tok->type() == TokenTypeStringLiteral;
		case ptt_char_constant:
			return tok->type() == TokenTypeCharacterConstant;
		case ptt_identifier: {
			return tok->type() == TokenTypeIdentifier;
		}
		case ptt_undefd_func_name: {
			Scope& s = _scopes.back();

			auto fit = s.functions.find(s.local_prefix() + tok->value());
			if (fit != s.functions.end() && fit->second->definition()) {
				return false;
			}

			return _peek(ptt_new_name);
		}
		case ptt_new_or_namespace_name: {
			return _peek(ptt_new_name) || _peek(ptt_namespace_name);
		}
		case ptt_namespace_name: {
			Scope& s = _scopes.back();

			return s.namespaces.count(s.local_prefix() + tok->value());
		}
		case ptt_new_name: {
			if (_peek(ptt_keyword)) {
				return false;
			}

			Scope& s = _scopes.back();

			if (s.types.count(s.local_prefix() + tok->value())) {
				return false;
			}

			if (s.variables.count(s.local_prefix() + tok->value())) {
				return false;
			}

			if (s.functions.count(s.local_prefix() + tok->value())) {
				return false;
			}

			return !s.namespaces.count(s.local_prefix() + tok->value());
		}
	}
	
	return false;
}

bool Parser::_peek(std::initializer_list<ParserTokenType> types) {
	TokenIterator prev_it = _cur_tok;
	for (ParserTokenType type : types) {
		if (!_peek(type)) {
			_cur_tok = prev_it;
			return false;
		}
		_consume(1);
	}
	_cur_tok = prev_it;
	return true;
}

Parser::Scope& Parser::_push_scope(const std::string& name) {
	Scope s = Scope(_scopes.back().global_prefix() + name + ".");
	s.return_type = _scopes.back().return_type;
	_scopes.push_back(s);
	return _scopes.back();
}

Parser::Scope& Parser::_push_scope(C3FunctionPtr function) {
	Scope& s = _push_scope(function->name());
	s.return_type = function->return_type();
	return s;
}

void Parser::_pop_scope() {
	if (_scopes.size() == 1) {
		// don't pop the global scope
		return;
	}

	_scopes.pop_back();
}

std::string Parser::_try_parse_full_name() {
	std::string ret = "";

	while (true) {
		if (!_peek(ptt_identifier)) {
			return ret;
		}
		ret += _token()->value();
		_consume(1);
		if (!_peek(ptt_namespace_delimiter)) {
			return ret;
		}
		ret += _token()->value();
		_consume(1);
	}

	return ret;
}

C3TypePtr Parser::_try_parse_type() {
	auto start = _cur_tok;

	auto name = _try_parse_full_name();

	if (name.empty()) { return nullptr; }

	C3TypePtr type = nullptr;

	for (auto it = _scopes.rbegin(); true; ++it) {
		if (it == _scopes.rend()) {
			_cur_tok = start;
			return nullptr;
		}
		auto& scope = *it;
		{
			auto t = scope.types.find(scope.local_prefix() + name);
			if (t != scope.types.end()) {
				type = t->second;
				break;
			}
		}
		{
			auto t = scope.types.find(name);
			if (t != scope.types.end()) {
				type = t->second;
				break;
			}
		}
	}
	
	while (_peek(ptt_asterisk)) {
		type = C3Type::PointerType(type);
		_consume(1);
	}

	return type;
}

C3VariablePtr Parser::_try_parse_variable() {
	auto start = _cur_tok;

	auto name = _try_parse_full_name();

	if (name.empty()) { return nullptr; }

	for (auto it = _scopes.rbegin(); it != _scopes.rend(); ++it) {
		auto& scope = *it;
		{
			auto vit = scope.variables.find(scope.local_prefix() + name);
			if (vit != scope.variables.end()) {
				return vit->second;
			}
		}
		{
			auto vit = scope.variables.find(name);
			if (vit != scope.variables.end()) {
				return vit->second;
			}
		}
	}

	_cur_tok = start;
	return nullptr;
}

C3FunctionPtr Parser::_try_parse_function() {
	auto start = _cur_tok;

	auto name = _try_parse_full_name();

	if (name.empty()) { return nullptr; }

	for (auto it = _scopes.rbegin(); it != _scopes.rend(); ++it) {
		auto& scope = *it;
		{
			auto fit = scope.functions.find(scope.local_prefix() + name);
			if (fit != scope.functions.end()) {
				return fit->second;
			}
		}
		{
			auto fit = scope.functions.find(name);
			if (fit != scope.functions.end()) {
				return fit->second;
			}
		}
	}

	_cur_tok = start;
	return nullptr;
}

ASTVariableDec* Parser::_parse_variable_dec(C3TypePtr type) {
	if (!_peek(ptt_new_name)) {
		_errors.push_back(ParseError("expected new variable name", _token()));
		return nullptr;
	}

	TokenPtr tok = _consume_token();

	Scope& scope = _scopes.back();

	C3VariablePtr var = C3VariablePtr(new C3Variable(type, tok->value(), scope.global_prefix() + tok->value(), tok));

	scope.variables[scope.local_prefix() + var->name()] = var;
	
	if (_peek(ptt_assignment)) {
		// initial value
		_consume(1); // consume '='
		ASTExpression* init = _parse_expression();
		if (!init) {
			return nullptr;
		}
		return new ASTVariableDec(var, init);
	}

	return new ASTVariableDec(var);
}

ASTNode* Parser::_parse_function_proto_or_def(C3TypePtr type, bool* was_just_proto) {
	// function prototype	
	bool args_are_named = false;
	TokenPtr protoTok = _token();
	ASTFunctionProto* proto = _parse_function_proto(type, &args_are_named);
	if (proto && _peek(ptt_open_brace)) {
		// function body
		ASTNode* node = nullptr;
		if (!args_are_named) {
			// unnamed arguments
			_errors.push_back(ParseError("function definition has unnamed arguments", _token()));
			// try to recover
			_consume(1);
			_push_scope(proto->func);
			delete _parse_block();
			_pop_scope();
		} else {
			// set up / parse the function body
			proto->func->set_definition(_token());
			_consume(1);
			_push_scope(proto->func);
			// add the arguments to the scope
			Scope& scope = _scopes.back();
			for (size_t i = 0; i < proto->arg_names.size(); ++i) {
				scope.variables[proto->arg_names[i]] = C3VariablePtr(new C3Variable(proto->func->arg_types()[i], proto->arg_names[i], scope.global_prefix() + proto->arg_names[i], protoTok));
			}
			// parse the body
			ASTSequence* body = _parse_block();
			if (body) {
				if (!_peek(ptt_close_brace)) {
					_errors.push_back(ParseError("expected closing brace", _token()));
					delete body;
				} else {
					_consume(1); // }
					node = new ASTFunctionDef(proto, body, scope.global_prefix());
				}
			}
			_pop_scope();
			// TODO: check for return statement
		}
		if (was_just_proto) {
			*was_just_proto = false;
		}
		return node;
	}
	
	// just the proto
	if (was_just_proto) {
		*was_just_proto = true;
	}
	return proto;
}

ASTFunctionProto* Parser::_parse_function_proto(C3TypePtr type, bool* args_are_named) {
	if (!_peek(ptt_undefd_func_name)) {
		_errors.push_back(ParseError("expected undefined function name", _token()));
		return nullptr;
	}

	TokenPtr tok = _consume_token();
	
	if (!_peek(ptt_open_paren)) {
		_errors.push_back(ParseError("expected open parenthesis", _token()));
		return nullptr;
	}

	_consume(1); // consume open parenthesis

	std::vector<C3TypePtr>   args;
	std::vector<std::string> names;
	
	while (!_peek(ptt_close_paren)) {
		C3TypePtr arg_type = _try_parse_type();

		if (!arg_type) {
			_errors.push_back(ParseError("expected argument type", _token()));
			return nullptr;
		}
		
		bool named = false;
		if (_peek(ptt_new_name)) {
			std::string name = _token()->value();
			named = true;
			for (std::string& n : names) {
				if (name == n) {
					_errors.push_back(ParseError("duplicate argument name", _token()));
					return nullptr;
				}
			}
			names.push_back(name);
			_consume(1);
		}
		
		args.push_back(arg_type);

		if (_peek(ptt_comma)) {
			_consume(1);
		} else if (!_peek(ptt_close_paren)) {
			_errors.push_back(ParseError(named ? "expected comma or end of argument list" : "expected comma, name, or end of argument list", _token()));
			return nullptr;
		}		
	}

	_consume(1); // consume close parenthesis

	if (args_are_named) {
		*args_are_named = (args.size() == names.size());
	}

	Scope& scope = _scopes.back();
	C3FunctionPtr func = C3FunctionPtr(new C3Function(type, tok->value(), scope.global_prefix() + tok->value(), std::move(args), tok));

	auto fit = scope.functions.find(tok->value());
	if (fit != scope.functions.end()) {
		if (func->signature() != fit->second->signature()) {
			_errors.push_back(ParseError("function has different signature than previous declaration", tok));
			return nullptr;
		}
		return new ASTFunctionProto(fit->second, names);
	}

	scope.functions[scope.local_prefix() + func->name()] = func;
	return new ASTFunctionProto(func, names);
}

ASTFunctionCall* Parser::_parse_function_call(ASTExpression* func) {
	if (func->type->type() != C3TypeTypeFunction) {
		_errors.push_back(ParseError("previous expression is not a function", _token()));
		return nullptr;
	}
	
	if (!_peek(ptt_open_paren)) {
		_errors.push_back(ParseError("expected '('", _token()));
		return nullptr;
	}
	_consume(1); // consume '('

	std::vector<ASTExpression*> args;
	const std::vector<C3TypePtr>& arg_types = func->type->signature().arg_types();

	for (size_t i = 0; i < arg_types.size(); ++i) {
		if (i > 0) {
			if (!_peek(ptt_comma)) {
				_errors.push_back(ParseError("expected ','", _token()));
				for (ASTExpression* exp : args) {
					delete exp;
				}
				return nullptr;
			}
			_consume(1); // consume comma
		}
		TokenPtr arg_tok = _token();
		ASTExpression* arg = _parse_expression();
		if (!arg) {
			for (ASTExpression* exp : args) {
				delete exp;
			}
			return nullptr;
		}
		if (*(arg->type) != *(arg_types[i])) {
			std::string msg = "invalid type for argument (expected '";
			msg += arg_types[i]->name() + "' but got '" + arg->type->name() + "'";
			_errors.push_back(ParseError(msg, arg_tok));
			// recover...
		}
		args.push_back(arg);
	}

	if (!_peek(ptt_close_paren)) {
		_errors.push_back(ParseError("expected ')'", _token()));
		for (ASTExpression* exp : args) {
			delete exp;
		}
		return nullptr;
	}
	_consume(1); // consume ')'

	return new ASTFunctionCall(func, args);
}

ASTNode* Parser::_parse_struct_dec_or_def() {
	// TODO: allow separate declarations / definitions

	if (!_peek(ptt_keyword_struct)) {
		return nullptr;
	}
	
	_consume(1); // struct
	
	if (!_peek(ptt_new_name)) {
		_errors.push_back(ParseError("expected new type name", _token()));
		return nullptr;
	}
	
	auto name = _consume_token();
	
	if (!_peek(ptt_open_brace)) {
		_errors.push_back(ParseError("expected opening brace", _token()));
		return nullptr;
	}
	
	_consume(1); // {
	
	std::vector<C3StructDefinition::MemberVariable> member_vars;
	
	_push_scope(name->value());
	while (!_peek(ptt_close_brace)) {
		C3TypePtr type = nullptr;
		if (!(type = _try_parse_type())) {
			_errors.push_back(ParseError("expected type", _token()));
			return nullptr;
		}
		if (!_peek(ptt_new_name)) {
			_errors.push_back(ParseError("expected new member name", _token()));
			return nullptr;
		}
		auto name = _consume_token();
		member_vars.emplace_back(name->value(), type);
		if (!_peek(ptt_semicolon)) {
			_errors.push_back(ParseError("expected semicolon", _token()));
			// try to recover
		} else {
			_consume(1); // ;
		}
	}
	_pop_scope();

	if (!_peek(ptt_close_brace)) {
		_errors.push_back(ParseError("expected closing brace", _token()));
		return nullptr;
	}

	_consume(1); // }

	Scope& scope = _scopes.back();
	scope.types[scope.local_prefix() + name->value()] = C3Type::StructType(name->value(), scope.global_prefix() + name->value(), C3StructDefinition(std::move(member_vars)));

	return new ASTNop();
}

ASTExpression* Parser::_parse_binop_rhs(ASTExpression* lhs) {
	TokenPtr tok = _consume_token();

	if (tok->type() != TokenTypePunctuator || tok->value() == ";") {
		_errors.push_back(ParseError("expected binary operator", _token()));
		delete lhs;
		return nullptr;
	}
	
	if (tok->value() == "." || tok->value() == "->") {
		if (tok->value() == "->") {
			if (lhs->type->type() != C3TypeTypePointer) {
				_errors.push_back(ParseError("dereferencing selection operator used on non-pointer type", _token()));
				delete lhs;
				return nullptr;
			}
			lhs = new ASTUnaryOp("*", lhs, lhs->type->points_to(), true);
		}
		auto type = lhs->type;
		if (type->type() != C3TypeTypeStruct) {
			_errors.push_back(ParseError(std::string("selection operator used on non-struct type '") + type->name() + "'", _token()));
			delete lhs;
			return nullptr;
		}
		if (!type->is_defined()) {
			_errors.push_back(ParseError("selection operator used on undefined struct", _token()));
			delete lhs;
			return nullptr;
		}
		auto member_vars = type->struct_definition().member_vars();
		for (size_t i = 0; i < member_vars.size(); ++i) {
			if (member_vars[i].name == _token()->value()) {
				_consume(1); // member name
				return new ASTStructMemberRef(lhs, i);
			}
		}
		_errors.push_back(ParseError("expected struct member", _token()));
		delete lhs;
		return nullptr;
	}
	
	auto precedence = _binary_ops[tok->value()];

	ASTExpression* rhs = _parse_expression(precedence);

	if (!rhs) {
		delete lhs;
		return nullptr;
	}

	C3TypePtr result_type = lhs->type;
	bool compatible = false;

	if (tok->value() == "==" || tok->value() == "!=" || tok->value() == "<" || tok->value() == "<=" || tok->value() == ">" || tok->value() == ">=") {
		compatible = ((lhs->type->is_floating_point() && rhs->type->is_floating_point()) || (lhs->type->is_integer() && rhs->type->is_integer()));
		result_type = C3Type::BoolType();
	} else if (*(lhs->type) == *(rhs->type)) {
		// values of the same type are compatible
		compatible = true;
	} else if (lhs->type->is_integer() && rhs->type->is_integer()) {
		// integers are compatible because they get promoted as necessary
		compatible = true;
		if (tok->value() != "=") {
			result_type = C3Type::Int64Type();
		}
	}

	if (!compatible) {
		std::string msg = "incompatible types to binary operator ('";
		msg += lhs->type->name() + "' and '" + rhs->type->name() + "')";
		_errors.push_back(ParseError(msg, tok));
		// try to recover...
	}

	return new ASTBinaryOp(tok->value(), lhs, rhs, result_type);
}

ASTExpression* Parser::_parse_inline_asm_operand(std::string* constraint) {
	if (!_peek(ptt_string_literal)) {
		_errors.push_back(ParseError("expected string literal constraint", _token()));	
		return nullptr;
	}

	// TODO: check constraints more closely
	
	if (_token()->value().find(',') != std::string::npos) {
		_errors.push_back(ParseError("invalid constraint", _token()));
		// recover...
	}
	
	TokenPtr ctok = _consume_token();
	if (ctok->value() == "m") {
		// make all memory operands indirect
		*constraint = "*m";
	} else if (ctok->value() == "=m") {
		// make all memory operands indirect
		*constraint = "=*m";
	} else {
		*constraint = ctok->value();
	}
	
	if (!_peek(ptt_open_paren)) {
		_errors.push_back(ParseError("expected '('", _token()));
		return nullptr;
	}
	_consume(1);

	TokenPtr etok = _token();
	ASTExpression* exp = _parse_expression();

	if (!exp) {
		return nullptr;
	}
	
	if ((*constraint)[0] == '*' && !exp->is_lvalue) {
		_errors.push_back(ParseError("operand must be lvalue for indirect constraint", etok));
		// recover...
	}

	if (!_peek(ptt_close_paren)) {
		_errors.push_back(ParseError("expected ')'", _token()));
		delete exp;
		return nullptr;
	}
	_consume(1);

	return exp;
}

ASTInlineAsm* Parser::_parse_inline_asm() {
	if (!_peek(ptt_keyword_asm)) {
		_errors.push_back(ParseError("expected 'asm'", _token()));
		return nullptr;
	}
	_consume(1);

	if (!_peek(ptt_open_paren)) {
		_errors.push_back(ParseError("expected '('", _token()));
		return nullptr;
	}
	_consume(1);
	
	if (!_peek(ptt_string_literal)) {
		_errors.push_back(ParseError("expected string literal assembly", _token()));	
		return nullptr;
	}
	std::string assembly = _consume_token()->value();
	
	bool failure = false;
	std::vector<ASTExpression*> outputs;
	std::vector<ASTExpression*> inputs;
	std::vector<std::string> constraints;

	// check for outputs
	if (_peek(ptt_colon)) {
		_consume(1); // consume the colon
		if (_peek(ptt_string_literal)) {
			// parse output operands
			while (true) {
				std::string constraint;
				TokenPtr optok = _token();
				ASTExpression* exp = _parse_inline_asm_operand(&constraint);
				if (!exp) {
					failure = true;
					break;
				}
				if (!exp->is_lvalue) {
					_errors.push_back(ParseError("output operand must be lvalue", optok));
					// try to recover...
				}
				constraints.push_back(constraint);
				if (constraint.find('*') != std::string::npos) {
					// indirect outputs are really inputs
					inputs.push_back(exp);
				} else {
					outputs.push_back(exp);
				}
				if (!_peek(ptt_comma)) {
					break;
				}
				_consume(1); // consume comma before continuing
			}
		}		
	}

	// check for inputs
	if (!failure && _peek(ptt_colon)) {
		_consume(1); // consume the colon
		if (_peek(ptt_string_literal)) {
			// parse input operands
			while (true) {
				std::string constraint;
				ASTExpression* exp = _parse_inline_asm_operand(&constraint);
				if (!exp) {
					failure = true;
					break;
				}
				constraints.push_back(constraint);
				inputs.push_back(exp);
				if (!_peek(ptt_comma)) {
					break;
				}
				_consume(1); // consume comma before continuing
			}
		}
	}

	// check for clobbers
	if (!failure && _peek(ptt_colon)) {
		_consume(1); // consume the colon
		if (_peek(ptt_string_literal)) {
			// parse clobbers
			while (true) {
				if (!_peek(ptt_string_literal)) {
					_errors.push_back(ParseError("expected string literal clobber", _token()));
					failure = true;
					break;
				}
				if (_token()->value().find(',') != std::string::npos) {
					_errors.push_back(ParseError("invalid clobber", _token()));
					// recover...
				}
				constraints.push_back("~{" + _consume_token()->value() + '}');
				if (!_peek(ptt_comma)) {
					break;
				}
				_consume(1); // consume comma before continuing
			}
		}
	}

	if (!failure) {
		if (!_peek(ptt_close_paren)) {
			_errors.push_back(ParseError("expected ')'", _token()));
			failure = true;
		} else {
			_consume(1);
		}
	}

	if (failure) {
		for (ASTExpression* exp : inputs) {
			delete exp;
		}
		for (ASTExpression* exp : outputs) {
			delete exp;
		}
		return nullptr;
	}

	return new ASTInlineAsm(assembly, outputs, inputs, constraints);
}

ASTReturn* Parser::_parse_return() {
	if (!_peek(ptt_keyword_return)) {
		_errors.push_back(ParseError("expected 'return'", _token()));
		return nullptr;
	}
	
	C3TypePtr expected_type = _scopes.back().return_type;
	
	if (!expected_type) {
		_errors.push_back(ParseError("unexpected return statement", _token()));
		// recover...
	}

	_consume(1); // consume 'return'

	// TODO: void returns
	
	ASTExpression* exp = _parse_expression();

	if (!exp) {
		return nullptr;
	}

	if (expected_type && *(exp->type) != *expected_type) {
		std::string msg = "invalid return type (expected '";
		msg += expected_type->name() + "' but got '" + exp->type->name() + "'";
		_errors.push_back(ParseError(msg, _token()));
		// recover...
	}
	
	return new ASTReturn(exp);
}

ASTExpression* Parser::_parse_primary() {
	if (auto var = _try_parse_variable()) {
		return new ASTVariableRef(var);
	} else if (auto func = _try_parse_function()) {
		return new ASTFunctionRef(func);
	} else if (_peek(ptt_number)) {
		// number
		TokenPtr tok = _consume_token();
		if (tok->value().find_first_of('.') != std::string::npos) {
			return new ASTFloatingPoint(atof(tok->value().c_str()), C3Type::DoubleType());
		} else {
			// TODO: support other bases
			return new ASTInteger(strtoll(tok->value().c_str(), NULL, 10), C3Type::Int64Type());
		}
	} else if (_peek(ptt_char_constant)) {
		// character constant
		TokenPtr tok = _consume_token();
		uint64_t value = 0;
		for (const char& c : tok->value()) {
			value <<= 8;
			value |= (unsigned char)c;
		}
		return new ASTInteger(value, C3Type::Int64Type());
	} else if (_peek(ptt_string_literal)) {
		// string literal
		TokenPtr tok = _consume_token();
		return new ASTConstantArray(tok->value().c_str(), tok->value().size(), C3Type::CharType());
	} else if (_peek(ptt_open_paren)) {
		// parenthesized expression
		_consume(1); // (
		auto exp = _parse_expression();
		if (!exp) {
			return nullptr;
		}
		if (!_peek(ptt_close_paren)) {
			_errors.push_back(ParseError("expected closing parenthesis", _token()));
			delete exp;
			return nullptr;
		}
		_consume(1); // )
		return exp;
	}

	_errors.push_back(ParseError("unexpected token", _token())); // intentionally vague
	return nullptr;
}

ASTExpression* Parser::_parse_expression(Precedence minPrecedence) {
	if (_peek(ptt_unary_op)) {
		// unary operation
		TokenPtr tok = _consume_token();
		TokenPtr rhs_tok = _token();
		ASTExpression* rhs = _parse_expression();
		if (!rhs) {
			return nullptr;
		}

		if (tok->value() == "&") {
			if (!rhs->is_lvalue) {
				_errors.push_back(ParseError("operand to '&' operator must be an lvalue", rhs_tok));
				delete rhs;
				return nullptr;
			}
			return new ASTUnaryOp(tok->value(), rhs, C3Type::PointerType(rhs->type));
		} else if (tok->value() == "*") {
			if (rhs->type->type() != C3TypeTypePointer) {
				_errors.push_back(ParseError("operand to '*' operator must be a pointer type", rhs_tok));
				delete rhs;
				return nullptr;
			}
			return new ASTUnaryOp(tok->value(), rhs, rhs->type->points_to(), true);
		}
		
		return new ASTUnaryOp(tok->value(), rhs, rhs->type);
	}

	ASTExpression* exp = _parse_primary();

	if (!exp) {
		return nullptr;
	}
	
	if (_peek(ptt_open_paren)) {
		// function call
		ASTExpression* call = _parse_function_call(exp);
		if (!call) {
			delete exp;
			return nullptr;
		}
		exp = call;
	}

	while (_peek(ptt_binary_op)) {
		auto precedence = _binary_ops[_token()->value()];
		
		if (precedence.rank < minPrecedence.rank || (precedence.rank == minPrecedence.rank && !minPrecedence.rtol)) {
			break;
		}
		
		if (!(exp = _parse_binop_rhs(exp))) {
			return nullptr;
		}
	}

	return exp;
}

ASTSequence* Parser::_parse_block() {
	ASTSequence* seq = new ASTSequence();

	while (true) {
		while (_peek(ptt_semicolon)) { _consume(1); }

		if (_peek(ptt_end_token) || _peek(ptt_close_brace)) {
			return seq;
		}

		ASTNode* node = _parse_statement();
		if (!node) {
			break;
		}

		seq->sequence.push_back(node);
	}
	
	delete seq;
	return nullptr;
}

ASTNode* Parser::_parse_statement() {
	ASTNode* node = nullptr;
	bool expect_semicolon = true;

	if (_peek(ptt_keyword_import)) {
		_consume(1); // import
		if (!_peek(ptt_new_or_namespace_name)) {
			_errors.push_back(ParseError("expected module name", _token()));
			return nullptr;
		}
		if (_scopes.size() > 1) {
			_errors.push_back(ParseError("imports can only be made in the global scope", _token()));
			return nullptr;
		}
		Scope& s = _scopes.back();
		if (!s.namespaces.empty()) {
			_errors.push_back(ParseError("imports can only be made in the top level namespace", _token()));
			return nullptr;
		}
		Preprocessor pp;
		// TODO: some sort of module searching
		if (!pp.process_file((std::string("modules/") + _token()->value() + "/" + _token()->value() + ".c3").c_str())) {
			_errors.push_back(ParseError("unable to import module", _token()));
			return nullptr;
		}
		auto name = _token()->value();
		_consume(1); // module name
		node = generate_ast(pp.tokens());
	} else if (_peek(ptt_keyword_namespace)) {
		_consume(1); // namespace
		if (!_peek(ptt_new_or_namespace_name)) {
			_errors.push_back(ParseError("expected namespace name", _token()));
			return nullptr;
		}
		auto name = _token()->value();
		_consume(1); // name
		if (!_peek(ptt_open_brace)) {
			_errors.push_back(ParseError("expected opening brace", _token()));
			return nullptr;
		}
		_consume(1); // {
		Scope& s = _scopes.back();
		s.namespaces.insert(s.local_prefix() + name);
		s.current_namespace.push_back(name);
		node = _parse_block();
		s.current_namespace.pop_back();
		if (!_peek(ptt_close_brace)) {
			_errors.push_back(ParseError("expected closing brace", _token()));
			delete node;
			return nullptr;
		}
		_consume(1); // }
		expect_semicolon = false;
	} else if (_peek(ptt_open_brace)) {
		// new code block
		_consume(1); // {
		_push_scope();
		node = _parse_block();
		_pop_scope();
		if (!_peek(ptt_close_brace)) {
			_errors.push_back(ParseError("expected closing brace", _token()));
			delete node;
			return nullptr;
		}
		_consume(1); // }
		expect_semicolon = false;
	} else if (C3TypePtr type = _try_parse_type()) {
		// proto or declaration
		if (_peek({ptt_identifier, ptt_open_paren})) {
			// function proto or def
			bool just_proto = true;
			node = _parse_function_proto_or_def(type, &just_proto);
			expect_semicolon = just_proto;
		} else {
			// variable declaration
			node = _parse_variable_dec(type);
		}
	} else if (_peek(ptt_keyword_if)) {
		// if block
		_consume(1); // if
		if (!_peek(ptt_open_paren)) {
			_errors.push_back(ParseError("expected opening parenthesis", _token()));
			return nullptr;
		}
		_consume(1); // (
		auto condition = _parse_expression();
		if (!condition) {
			return nullptr;
		}
		if (!_peek(ptt_close_paren)) {
			_errors.push_back(ParseError("expected closing parenthesis", _token()));
			delete condition;
			return nullptr;
		}
		_consume(1); // )
		_push_scope();
		auto truePath = _parse_statement();
		_pop_scope();
		if (!truePath) {
			delete condition;
			return nullptr;
		}
		ASTNode* falsePath = nullptr;
		if (_peek(ptt_keyword_else)) {
			_consume(1); // else
			_push_scope();
			falsePath = _parse_statement();
			_pop_scope();
			if (!falsePath) {
				delete condition;
				delete truePath;
				return nullptr;
			}
		}
		node = new ASTCondition(condition, truePath, falsePath ? falsePath : new ASTSequence());
		expect_semicolon = false;
	} else if (_peek(ptt_keyword_while)) {
		// while loop
		_consume(1); // if
		if (!_peek(ptt_open_paren)) {
			_errors.push_back(ParseError("expected opening parenthesis", _token()));
			return nullptr;
		}
		_consume(1); // (
		auto condition = _parse_expression();
		if (!condition) {
			return nullptr;
		}
		if (!_peek(ptt_close_paren)) {
			_errors.push_back(ParseError("expected closing parenthesis", _token()));
			delete condition;
			return nullptr;
		}
		_consume(1); // )
		_push_scope();
		auto body = _parse_statement();
		_pop_scope();
		if (!body) {
			delete condition;
			return nullptr;
		}
		node = new ASTWhileLoop(condition, body);
		expect_semicolon = false;
	} else if (_peek(ptt_keyword_asm)) {
		// inline assembly
		node = _parse_inline_asm();
	} else if (_peek(ptt_keyword_return)) {
		// return statement
		node = _parse_return();
	} else if (_peek(ptt_keyword_struct)) {
		// struct declaration or definition
		node = _parse_struct_dec_or_def();
		expect_semicolon = false;
	} else {
		// expression
		node = _parse_expression();
	}

	if (node && expect_semicolon && !_peek(ptt_semicolon)) {
		_errors.push_back(ParseError("expected semicolon", _token()));
		// try to continue anyways
	}
	
	return node;
}

