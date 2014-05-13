#ifndef C3_FUNCTION_SIGNATURE_H
#define C3_FUNCTION_SIGNATURE_H

#include <vector>
#include <string>

class C3Type;
typedef std::shared_ptr<C3Type> C3TypePtr;

class C3FunctionSignature {
	public:
		C3FunctionSignature();
		C3FunctionSignature(C3TypePtr return_type, const std::vector<C3TypePtr>&& arg_types);

		C3TypePtr return_type() const;
		const std::vector<C3TypePtr>& arg_types() const;
		
		const std::string& string() const;

		bool operator==(C3FunctionSignature other) const;
		bool operator!=(C3FunctionSignature other) const;
		
	private:
		C3TypePtr _return_type;
		std::vector<C3TypePtr> _arg_types;

		std::string _string;
};

#endif