#include "../shared.h"

struct tDBNode;
tDBNode* pRootNode = nullptr;
size_t nNumNodes = 0;
std::string GetFullPathForDBNode(int id);

struct __attribute__((packed, aligned(1))) tDBValue {
	uint32_t pNameString;	// +0
	uint8_t valueType;		// +4
	uint16_t size;			// +5 array size included
	uint8_t arrayType;		// +7
	uint32_t dataPtr;		// +8 nulled by the game's reader, only used at runtime
	char data[0];			// +C

	const char* GetName() const {
		return (const char*)pNameString;
	}

	size_t GetValueTypeSize() const {
		return GetDBValueTypeSize(valueType);
	}

	auto GetAsChar(int offset) {
		auto addr = &data[offset];
		return *(unsigned char*)addr;
	}

	auto GetAsShort(int offset) {
		auto addr = &data[offset * 2];
		return *(unsigned short*)addr;
	}

	auto GetAsInt(int offset) {
		auto addr = &data[offset * 4];
		return *(int*)addr;
	}

	auto GetAsFloat(int offset) {
		auto addr = &data[offset * 4];
		return *(float*)addr;
	}

	auto GetAsString(int offset) {
		auto addr = &data[offset];
		return (char*)addr;
	}

	void WriteValueToFile(std::ofstream& outFile, int index) {
		switch (valueType) {
			case DBVALUE_CHAR: {
				outFile << (int)GetAsChar(index);
			} break;
			case DBVALUE_STRING: {
				outFile << "\"";
				outFile << GetAsString(index);
				outFile << "\"";
			} break;
			case DBVALUE_BOOL: {
				outFile << (GetAsInt(index) != 0 ? "true" : "false");
			} break;
			case DBVALUE_INT: {
				outFile << GetAsInt(index);
			} break;
			case DBVALUE_FLOAT: {
				auto value = GetAsFloat(index);
				if (std::abs(value) < 0.00001) value = 0;
				outFile << value;
			} break;
			case DBVALUE_RGBA: {
				outFile << "{ ";
				for (int i = 0; i < 4; i++) {
					outFile << (int)GetAsChar((index * 4) + i);
					if (i < 4 - 1) outFile << ", ";
				}
				outFile << " }";
			} break;
			case DBVALUE_VECTOR2:
			case DBVALUE_VECTOR3:
			case DBVALUE_VECTOR4: {
				int valueCount = (valueType - DBVALUE_VECTOR2) + 2;
				outFile << "{ ";
				for (int i = 0; i < valueCount; i++) {
					auto value = GetAsFloat((index * valueCount) + i);
					if (std::abs(value) < 0.00001) value = 0;
					outFile << value;
					if (i < valueCount - 1) outFile << ", ";
				}
				outFile << " }";
			} break;
			case DBVALUE_NODE: {
				outFile << "\"";
				outFile << GetFullPathForDBNode(GetAsShort(index));
				outFile << "\"";
			} break;
			default: {
				WriteConsole("WARNING: Unknown value type " + std::to_string(valueType) + " for " + GetName());
				outFile << "*UNKNOWN*";
			} break;
		}
	}

	void WriteToFile(std::ofstream& outFile) {
		outFile << aValueTypeNames[valueType];
		// const char* for variable strings
		if (valueType == DBVALUE_STRING && arrayType == DBARRAY_VARIABLE) {
			outFile << "*";
		}
		outFile << " ";
		auto name = (std::string)GetName();
		std::replace(name.begin(), name.end(), '[', '(');
		std::replace(name.begin(), name.end(), ']', ')');
		outFile << name;
		if (arrayType == DBARRAY_FIXED) {
			// const char[i] for fixed strings
			if (valueType == DBVALUE_STRING) {
				outFile << "[";
				outFile << size;
				outFile << "]";
			}
			else outFile << "[]";
		}
		outFile << " = ";

		// one-liner if it's just one value, else one line per entry
		if (arrayType == DBARRAY_FIXED && valueType != DBVALUE_STRING) {
			outFile << "{\n";
			if (size % GetValueTypeSize() != 0) {
				WriteConsole("WARNING: Bad array size for " + (std::string)GetName() + " (" + std::to_string(size) + ", not divisible by " + std::to_string(GetValueTypeSize()) + ")");
			}
			for (int i = 0; i < size / GetValueTypeSize(); i++) {
				outFile << "\t";
				WriteValueToFile(outFile, i);
				if (i < (size / GetValueTypeSize()) - 1) outFile << ",\n";
			}
			outFile << "\n}";
		}
		else {
			if (valueType != DBVALUE_STRING && size != GetValueTypeSize()) {
				WriteConsole("WARNING: Bad size for " + (std::string)GetName() + " (" + std::to_string(size) + ", expected " + std::to_string(GetValueTypeSize()) + ")");
			}
			WriteValueToFile(outFile, 0);
		}
		outFile << ";\n";
	}

	void ParseFileToMemory() {
		if (pNameString) {
			pNameString += (uint32_t)this;
		}
		dataPtr = 0;
	}
};

bool DoesNodeHaveChildren(tDBNode* node);

struct tDBNode {
	uint32_t vtable;			// +0
	int16_t parentOffset;		// +4
	int16_t lastChildOffset;	// +6
	int16_t prevNodeOffset;		// +8 usually -1, 0 if it's the first one, amount of nodes to get to the previous one in the folder
	uint16_t dataCount;			// +A
	uint32_t pNameString;		// +C
	uint32_t pValues;			// +10

	bool DoesAnythingDependOnMe() {
		for (int i = 0; i < nNumNodes; i++) {
			if (pRootNode[i].GetParent() == this) return true;
		}
		return false;
	}

	tDBNode* GetParent() {
		return this + parentOffset;
	}

	const char* GetName() const {
		return (const char*)pNameString;
	}

	tDBValue* GetValue(int id) {
		if (id >= dataCount) return nullptr;

		auto value = pValues;
		for (int i = 0; i < id; i++) {
			value += ((tDBValue*)value)->size + 0xC; // size + data
		}
		return (tDBValue*)value;
	}

	std::string GetFullPath() {
		std::string filePath = GetName();
		if (this == GetParent()) return filePath; // root node, no parent

		auto parent = GetParent();
		while (parent != parent->GetParent()) {
			filePath = parent->GetName() + (std::string)"/" + filePath;
			parent = parent->GetParent();
		}
		filePath = parent->GetName() + (std::string)"/" + filePath;
		return filePath;
	}

	void ParseFileToMemory() {
		if (pNameString) {
			pNameString += (uint32_t)this;
		}

		if (pValues) {
			pValues += (uint32_t)this;
			for (int i = 0; i < dataCount; i++) {
				GetValue(i)->ParseFileToMemory();
			}
		}
	}

	void WriteToFile(const std::string& outFolder) {
		auto filePath = outFolder + "/" + GetFullPath();
		if (DoesAnythingDependOnMe()) std::filesystem::create_directory(filePath);

		if (dataCount > 0 || !DoesNodeHaveChildren(this)) {
			auto outFile = std::ofstream(filePath + ".h");
			for (int j = 0; j < dataCount; j++) {
				GetValue(j)->WriteToFile(outFile);
			}
		}
	}
};

bool DoesNodeHaveChildren(tDBNode* node) {
	for (int i = 0; i < nNumNodes; i++) {
		if (node[i].GetParent() == node) return true;
	}
	return false;
}

std::string GetFullPathForDBNode(int id) {
	return pRootNode[id].GetFullPath();
}

void ParseDBData(tDBNode* data, int count, const char* fileName) {
	pRootNode = data;
	nNumNodes = count;

	WriteConsole("Parsing...");
	for (int i = 0; i < count; i++) {
		data[i].ParseFileToMemory();
	}
	WriteConsole("Parsed");

	WriteConsole("Extracting...");
	auto outFolder = fileName + (std::string)" extracted";
	std::filesystem::create_directory(outFolder);
	for (int i = 0; i < count; i++) {
		data[i].WriteToFile(outFolder);
	}
	WriteConsole("Database extracted");
}

bool ParseDB(const char* fileName) {
	std::ifstream fin(fileName, std::ios::in | std::ios::binary );
	if (!fin.is_open()) return false;

	fin.seekg(0, std::ios::end);

	size_t fileSize = fin.tellg();

	tDBHeader header;
	if (fileSize <= sizeof(header)) return false;

	fin.seekg(0, std::ios::beg);
	fin.read((char*)&header, sizeof(header));

	auto data = new char[fileSize - sizeof(header)];

	// PDB1
	if (header.identifier != 0x1A424450 || header.version != 512 || header.numNodes == 0) return false;
	fin.read(data, fileSize - sizeof(header));

	ParseDBData((tDBNode*)data, header.numNodes, fileName);
	return true;
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		WriteConsole("Usage: FlatOut2DBExtractor_gcp.exe <filename>");
		return 0;
	}
	if (!ParseDB(argv[1])) {
		WriteConsole("Failed to load binary database " + (std::string)argv[1] + "!");
	}
	return 0;
}
