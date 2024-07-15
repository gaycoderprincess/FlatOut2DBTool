#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdint>

void WriteConsole(const std::string& str) {
	static auto& out = std::cout;
	out << str;
	out << "\n";
	out.flush();
}

enum eDBValueType {
	DBVALUE_CHAR = 1,
	DBVALUE_STRING = 2,
	DBVALUE_BOOL = 5,
	DBVALUE_INT = 6,
	DBVALUE_FLOAT = 7,
	DBVALUE_RGBA = 8,
	DBVALUE_VECTOR2 = 9,
	DBVALUE_VECTOR3 = 10,
	DBVALUE_VECTOR4 = 11,
	DBVALUE_NODEARRAY = 12,
};
const char* aValueTypeNames[] = {
		nullptr,
		"char",
		"const char",
		nullptr,
		nullptr,
		"bool",
		"int",
		"float",
		"rgba",
		"vec2",
		"vec3",
		"vec4",
		"node*",
};

enum eDBArrayType {
	DBARRAY_SINGLE,
	DBARRAY_FIXED,
	DBARRAY_VARIABLE, // used for strings
};

struct tDBHeader {
	uint32_t identifier;
	uint32_t version;
	uint32_t numNodes;
};