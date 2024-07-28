#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdint>
#include <algorithm>

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
	DBVALUE_NODE = 12,
	DBVALUE_MAX_COUNT
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

bool IsDBTypeVector(int type) {
	return type >= DBVALUE_VECTOR2 && type <= DBVALUE_VECTOR4;
}

size_t GetDBValueTypeSize(int type) {
	switch (type) {
		case DBVALUE_CHAR:
		case DBVALUE_STRING:
			return 1;
		case DBVALUE_NODE:
			return 2;
		case DBVALUE_BOOL:
		case DBVALUE_RGBA:
		case DBVALUE_INT:
		case DBVALUE_FLOAT:
			return 4;
		case DBVALUE_VECTOR2:
			return 4 * 2;
		case DBVALUE_VECTOR3:
			return 4 * 3;
		case DBVALUE_VECTOR4:
			return 4 * 4;
		default:
			return 0;
	}
}

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