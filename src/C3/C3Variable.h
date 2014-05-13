#pragma once

#include "C3Type.h"
#include "../Token.h"

#include <string>
#include <memory>

class C3Variable {
	public:
		C3Variable(C3TypePtr type, const std::string& name, const std::string& global_name, TokenPtr declaration) : _type(type), _name(name), _global_name(global_name), _declaration(declaration) {}
			
		C3TypePtr type() { return _type; }
		const std::string& name() { return _name; }
		const std::string& global_name() { return _global_name; }
		TokenPtr declaration() { return _declaration; }
			
	private:
		C3TypePtr _type;
		std::string _name;
		std::string _global_name;
		TokenPtr _declaration;
};

typedef std::shared_ptr<C3Variable> C3VariablePtr;
