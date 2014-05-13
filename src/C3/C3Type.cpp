#include "C3Type.h"

#include <assert.h>

C3Type::C3Type(const std::string& name, const std::string& global_name, C3TypeType type) : _name(name), _global_name(global_name), _type(type), _is_defined(false) {
}

C3Type::C3Type(const std::string& name, C3TypeType type) : _name(name), _global_name(name), _type(type), _is_defined(false) {
}

C3Type::C3Type(const std::string& name, C3TypeType type, C3TypePtr points_to) : _name(name), _global_name(name), _type(type), _points_to(points_to), _is_defined(false) {
	assert(type == C3TypeTypePointer);
}

C3Type::C3Type(const C3FunctionSignature& signature) : _name(signature.string()), _global_name(_name), _type(C3TypeTypeFunction), _function_sig(signature), _is_defined(false) {
}

size_t C3Type::size() const {
	switch (_type) {
		case C3TypeTypePointer:
		case C3TypeTypeFunction:
			return sizeof(void*);
		case C3TypeTypeStruct:
			return _is_defined ? _struct_def.size() : 0;
		case C3TypeTypeBuiltInVoid:
			return 0;
		case C3TypeTypeBuiltInBool:
		case C3TypeTypeBuiltInInt8:
			return 1;
		case C3TypeTypeBuiltInInt32:
			return 8;
		case C3TypeTypeBuiltInInt64:
			return 8;
		case C3TypeTypeBuiltInDouble:
			return sizeof(double);
	}
	
	assert(false);
	return 0;
}

C3TypePtr C3Type::points_to() const {
	return _points_to;
}

const C3FunctionSignature& C3Type::signature() const {
	return _function_sig;
}

bool C3Type::is_integer() const {
	return (_type == C3TypeTypeBuiltInInt8 || _type == C3TypeTypeBuiltInInt32 || _type == C3TypeTypeBuiltInInt64);
}

bool C3Type::is_floating_point() const {
	return (_type == C3TypeTypeBuiltInDouble);
}

bool C3Type::is_signed() const {
	// TODO: add signedness specifier
	return true;
}

bool C3Type::is_defined() const {
	return (_type != C3TypeTypeStruct || _is_defined);
}

void C3Type::define(const C3StructDefinition& definition) {
	_struct_def = definition;
	_is_defined = true;
}

bool C3Type::operator==(const C3Type& right) const {
	return (type() == right.type() && (type() != C3TypeTypePointer || *(points_to()) == *(right.points_to())));
}

bool C3Type::operator!=(const C3Type& right) const {
	return !(*this == right);
}

C3TypePtr C3Type::PointerType(C3TypePtr type) {
	if (!type->_pointer) {
		type->_pointer = C3TypePtr(new C3Type(type->name() + "*", C3TypeTypePointer, type));
	}
	return type->_pointer;
}

C3TypePtr C3Type::VoidType() {
	static C3TypePtr ret = C3TypePtr(new C3Type("void", C3TypeTypeBuiltInVoid));
	return ret;
}

C3TypePtr C3Type::BoolType() {
	static C3TypePtr ret = C3TypePtr(new C3Type("bool", C3TypeTypeBuiltInBool));
	return ret;
}

C3TypePtr C3Type::FunctionType(const C3FunctionSignature& signature) {
	return C3TypePtr(new C3Type(signature));
}

C3TypePtr C3Type::StructType(const std::string& name, const std::string& global_name) {
	return C3TypePtr(new C3Type(name, global_name, C3TypeTypeStruct));
}

C3TypePtr C3Type::StructType(const std::string& name, const std::string& global_name, const C3StructDefinition& definition) {
	auto type = C3TypePtr(new C3Type(name, global_name, C3TypeTypeStruct));
	type->define(definition);
	return type;
}

C3TypePtr C3Type::CharType() {
	static C3TypePtr ret = C3TypePtr(new C3Type("char", C3TypeTypeBuiltInInt8));
	return ret;
}

C3TypePtr C3Type::Int32Type() {
	static C3TypePtr ret = C3TypePtr(new C3Type("int32", C3TypeTypeBuiltInInt32));
	return ret;
}

C3TypePtr C3Type::Int64Type() {
	static C3TypePtr ret = C3TypePtr(new C3Type("int64", C3TypeTypeBuiltInInt64));
	return ret;
}

C3TypePtr C3Type::DoubleType() {
	static C3TypePtr ret = C3TypePtr(new C3Type("double", C3TypeTypeBuiltInDouble));
	return ret;
}
