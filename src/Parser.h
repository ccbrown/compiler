#pragma once

#include "Token.h"
#include "AST.h"
#include "C3/C3.h"

#include <list>
#include <unordered_map>
#include <unordered_set>
#include <string>

struct ParseError {
	ParseError(const std::string& msg, TokenPtr tok) : message(msg), token(tok) {}
	
	std::string message;
	TokenPtr token;
};

class Parser {
	public:
		Parser();

		ASTSequence* generate_ast(const std::list<TokenPtr>& tokens);
		const std::list<ParseError>& errors();

	private:
		enum ParserTokenType {
			ptt_open_angle,
			ptt_close_angle,
			ptt_semicolon,
			ptt_colon,
			ptt_open_brace,
			ptt_close_brace,
			ptt_open_paren,
			ptt_close_paren,
			ptt_comma,
			ptt_asterisk,
			ptt_ampersand,
			ptt_assignment,
			ptt_equality,
			ptt_inequality,
			ptt_namespace_delimiter,
			ptt_type,
			ptt_type_name,
			ptt_new_namespace_name,
			ptt_new_variable_name,
			ptt_new_type_name,
			ptt_local_type_name,
			ptt_undefd_func_name,
			ptt_number,
			ptt_end_token,
			ptt_unary_op,
			ptt_binary_op,
			ptt_identifier,
			ptt_string_literal,
			ptt_char_constant,
			ptt_keyword,
			ptt_keyword_asm,
			ptt_keyword_auto,
			ptt_keyword_class,
			ptt_keyword_const,
			ptt_keyword_extern,
			ptt_keyword_return,
			ptt_keyword_import,
			ptt_keyword_if,
			ptt_keyword_else,
			ptt_keyword_while,
			ptt_keyword_static,
			ptt_keyword_static_cast,
			ptt_keyword_namespace,
			ptt_keyword_nullptr,
		};
		
		struct Scope {
			Scope() : prefix("") {}
			Scope(std::string prefix) : prefix(prefix) {}
				
			std::string global_prefix() {
				return prefix + local_prefix();
			}
			
			std::string local_prefix() {
				std::string ret = "";
				for (auto& name : current_namespace) {
					ret += name + "::";
				}
				return ret;
			}
			
			std::string prefix;
			std::unordered_map<std::string, C3TypePtr> types;
			std::unordered_map<std::string, C3VariablePtr> variables;
			std::unordered_map<std::string, C3FunctionPtr> functions;
			std::list<std::string> current_namespace;
			std::unordered_set<std::string> namespaces;
			C3TypePtr return_type;
		};
		
		std::list<Scope> _scopes;
		
		struct Precedence {
			int  rank;
			bool rtol; // right to left associativity if true, unary operators are always right to left regardless
		};
		
		std::unordered_map<std::string, Precedence> _unary_ops;
		std::unordered_map<std::string, Precedence> _binary_ops;

		std::unordered_set<std::string> _keywords;

		std::unordered_set<std::string> _imported_modules;

		typedef std::list<TokenPtr>::const_iterator TokenIterator;
		
		TokenIterator _cur_tok;
		TokenIterator _end_tok;

		TokenPtr _token();

		void _consume(size_t tokens);
		TokenPtr _consume_token();

		bool _peek(ParserTokenType type, TokenIterator* next = nullptr);
		bool _peek(std::initializer_list<ParserTokenType> types);

		Scope& _push_scope(const std::string& name = "");
		Scope& _push_scope(C3FunctionPtr function);
		void _pop_scope();

		std::string _try_parse_full_name();

		C3TypePtr _resolve_type(const std::string& name);
		C3TypePtr _try_parse_type();

		C3VariablePtr _resolveVariable(const std::string& name);
		C3VariablePtr _try_parse_variable();

		C3FunctionPtr _resolveFunction(const std::string& name);
		C3FunctionPtr _try_parse_function();
		
		/**
		* Takes ownership of `from` only if successful.
		*/
		ASTExpression* _implicit_conversion(ASTExpression* from, C3TypePtr to);

		/**
		* Takes ownership of `from` only if successful.
		*/
		ASTExpression* _explicit_conversion(ASTExpression* from, C3TypePtr to);
		
		C3TypePtr _resolve_auto_type(C3TypePtr auto_type, C3TypePtr target);

		ASTVariableDec* _parse_variable_dec();
		ASTNode* _parse_function_proto_or_def(bool* was_just_proto);
		ASTFunctionProto* _parse_function_proto(bool* args_are_named = nullptr);
		ASTFunctionCall* _parse_function_call(ASTExpression* func);
		ASTNode* _parse_class_dec_or_def();

		ASTExpression* _parse_expression(Precedence minPrecedence = { 0, false });
		ASTExpression* _parse_primary();
		ASTCast* _parse_static_cast();

		ASTExpression* _parse_inline_asm_operand(std::string* constraint);
		ASTInlineAsm* _parse_inline_asm();
		
		ASTNode* _parse_external_declaration();

		ASTReturn* _parse_return();

		ASTExpression* _parse_binop_rhs(ASTExpression* lhs);
		ASTSequence* _parse_block();
		ASTNode* _parse_statement();

		std::list<ParseError> _errors;
};
