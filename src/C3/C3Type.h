#ifndef C3_TYPE_H
#define C3_TYPE_H

#include <string>
#include <memory>

enum C3TypeType {
	C3TypeTypePointer,
	C3TypeTypeFunction,
	C3TypeTypeStruct,
	C3TypeTypeBuiltInVoid,
	C3TypeTypeBuiltInBool,
	C3TypeTypeBuiltInInt8,
	C3TypeTypeBuiltInInt32,
	C3TypeTypeBuiltInInt64,
	C3TypeTypeBuiltInDouble,
};

class C3Type;
typedef std::shared_ptr<C3Type> C3TypePtr;

#include "C3FunctionSignature.h"
#include "C3StructDefinition.h"

class C3Type {
	public:
		const std::string& name() const { return _name; }
		const std::string& global_name() const { return _global_name; }
		C3TypeType type() const { return _type; }
		size_t size() const;

		C3TypePtr points_to() const;
		const C3FunctionSignature& signature() const;

		bool is_integer() const;
		bool is_floating_point() const;
		bool is_signed() const;
		bool is_defined() const;

		void define(const C3StructDefinition& definition);
		const C3StructDefinition& struct_definition() const { return _struct_def; }

		bool operator==(const C3Type& right) const;
		bool operator!=(const C3Type& right) const;

		static C3TypePtr PointerType(C3TypePtr type);
		static C3TypePtr FunctionType(const C3FunctionSignature& signature);
		static C3TypePtr StructType(const std::string& name, const std::string& global_name);
		static C3TypePtr StructType(const std::string& name, const std::string& global_name, const C3StructDefinition& definition);
		static C3TypePtr VoidType();
		static C3TypePtr BoolType();
		static C3TypePtr CharType();
		static C3TypePtr Int32Type();
		static C3TypePtr Int64Type();
		static C3TypePtr DoubleType();

	private:
		C3Type(const std::string& name, const std::string& global_name, C3TypeType type);
		C3Type(const std::string& name, C3TypeType type);
		C3Type(const std::string& name, C3TypeType type, C3TypePtr pointsTo);
		C3Type(const C3FunctionSignature& signature);

		std::string _name;
		std::string _global_name;
		C3TypeType _type;

		C3TypePtr _pointer;
		C3TypePtr _points_to;
		
		C3FunctionSignature _function_sig;		

		bool _is_defined;
		C3StructDefinition _struct_def;
};

#endif