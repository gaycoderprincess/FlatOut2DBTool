#include <vector>
#include <cstring>
#include "../shared.h"

struct __attribute__((packed, aligned(1))) tDBValue {
	uint32_t pNameString = 0;	// +0
	uint8_t valueType = 0;		// +4
	uint16_t size = 0;			// +5 array size included
	uint8_t arrayType = 0;		// +7
	uint32_t dataPtr = 0;		// +8 nulled by the game's reader, only used at runtime
};

struct tDBNode {
	uint32_t vtable = 0;		// +0
	int16_t parentOffset = 0;	// +4
	int16_t lastChildOffset = 0;// +6
	int16_t prevNodeOffset = 0;	// +8 usually -1, 0 if it's the first one, amount of nodes to get to the previous one in the folder
	uint16_t dataCount = 0;		// +A
	uint32_t pNameString = 0;	// +C
	uint32_t pValues = 0;		// +10
};

tDBHeader gHeader;

struct tDBValueTemp {
	std::string name;
	int type = 0;
	int arrayCount = 0;
	void* data = nullptr;
	size_t baseFilePosition;
	size_t nameFilePosition;
};

struct tDBNodeTemp {
	std::filesystem::path fullPath;
	std::string name;
	std::vector<tDBValueTemp> values;
	int parentNodeId = 0;
	size_t baseFilePosition = 0;
	size_t nameFilePosition = 0;
	size_t valuesFilePosition = 0;
};
std::vector<tDBNodeTemp> aNodes;

std::filesystem::path dbBaseFolderPath;

tDBNodeTemp* GetNodeForPath(const std::filesystem::path& path, bool createNew) {
	for (auto& node : aNodes) {
		if (node.fullPath == path) return &node;
	}

	if (!createNew) return nullptr;
	aNodes.push_back({});
	auto node = &aNodes[aNodes.size()-1];
	node->fullPath = path;
	node->name = path.filename().string();
	return node;
}

void GenerateNodesToGetToRoot(const std::filesystem::path& path) {
	auto node = GetNodeForPath(path, true);
	auto currPath = path;
	while (currPath != aNodes[0].fullPath) {
		currPath = currPath.parent_path();
		auto parentNode = GetNodeForPath(currPath, true);
		node->parentNodeId = parentNode - &aNodes[0];
		node = parentNode;
	}
}

std::string GetSectionOfString(const std::string& in, size_t start, size_t len) {
	std::string out = in;
	if (start > 0) out.erase(out.begin() + start);
	while (out.length() > len) out.pop_back();
	return out;
}

tDBNodeTemp* GetDBValueNodePtr(std::string string) {
	if (!string.starts_with("\"")) return nullptr;
	string.erase(string.begin());
	if (string.ends_with("\"")) {
		string.pop_back();
	}
	else if (string.ends_with("\",")) {
		string.pop_back();
		string.pop_back();
	}
	else return nullptr;

	auto pNode = GetNodeForPath(dbBaseFolderPath.string() + "/" + string, false);
	if (!pNode) {
		WriteConsole("ERROR: Failed to find node " + dbBaseFolderPath.string() + "/" + string);
		return nullptr;
	}
	return pNode;
}

template<typename T>
bool GetDBValueVector(std::string string, T* data, int valueCount) {
	string.erase(string.begin(), string.begin() + 2);

	for (int i = 0; i < valueCount; i++) {
		data[i] = std::stof(string);

		// find next value
		if (i + 1 < valueCount) {
			auto next = string.find(", ");
			if (next == std::string::npos) return false;
			string.erase(string.begin(), string.begin() + next + 2);
		}
	}
	return true;
}

bool ReadSingleDBValue(tDBValueTemp* value, int type, const std::string& string) {
	switch (type) {
		case DBVALUE_INT: {
			value->data = new int;
			*(int *) value->data = std::stoi(string);
		} break;
		case DBVALUE_FLOAT: {
			value->data = new float;
			*(float *) value->data = std::stof(string);
		} break;
		case DBVALUE_BOOL: {
			value->data = new uint32_t;
			*(uint32_t *) value->data = string.starts_with("true");
		} break;
		case DBVALUE_NODE: {
			value->data = new tDBNodeTemp*;
			*(tDBNodeTemp**)value->data = GetDBValueNodePtr(string);
		} break;
		case DBVALUE_VECTOR2:
		case DBVALUE_VECTOR3:
		case DBVALUE_VECTOR4: {
			if (!string.starts_with("{ ")) {
				WriteConsole("ERROR: Failed to find vector in " + string);
				return false;
			}

			int valueCount = (type - DBVALUE_VECTOR2) + 2;
			value->data = new float[valueCount];
			GetDBValueVector<float>(string, (float*)value->data, valueCount);
		} break;
		case DBVALUE_RGBA: {
			int valueCount = 4;
			value->data = new uint8_t[valueCount];
			GetDBValueVector<uint8_t>(string, (uint8_t*)value->data, valueCount);
		} break;
		default: {
			WriteConsole("ERROR: type not implemented: " + std::to_string(type));
			return false;
		}
	}
	return true;
}

bool ReadDBArrayNextLine(std::ifstream& file, std::string& outString) {
	if (!std::getline(file, outString)) return false;
	if (outString.ends_with("};")) return false;
	while (outString.starts_with("\t")) {
		outString.erase(outString.begin());
	}
	return true;
}

bool ReadDBArrayValue(tDBValueTemp* value, int type, std::ifstream& file, std::string string) {
	if (!string.ends_with("{")) {
		return false;
	}

	switch (type) {
		case DBVALUE_CHAR: {
			std::vector<uint8_t> values;

			while (ReadDBArrayNextLine(file, string)) {
				values.push_back(std::stoi(string));
			}

			value->data = new uint8_t[values.size()];
			value->arrayCount = values.size();
			auto arr = (uint8_t *)value->data;
			for (int j = 0; j < value->arrayCount; j++) {
				arr[j] = values[j];
			}
		} break;
		case DBVALUE_INT: {
			std::vector<int> values;

			while (ReadDBArrayNextLine(file, string)) {
				values.push_back(std::stoi(string));
			}

			value->data = new int[values.size()];
			value->arrayCount = values.size();
			auto arr = (int *)value->data;
			for (int j = 0; j < value->arrayCount; j++) {
				arr[j] = values[j];
			}
		} break;
		case DBVALUE_FLOAT: {
			std::vector<float> values;

			while (ReadDBArrayNextLine(file, string)) {
				values.push_back(std::stof(string));
			}

			value->data = new float[values.size()];
			value->arrayCount = values.size();
			auto arr = (float *)value->data;
			for (int j = 0; j < value->arrayCount; j++) {
				arr[j] = values[j];
			}
		} break;
		case DBVALUE_NODE: {
			std::vector<tDBNodeTemp*> values;

			while (ReadDBArrayNextLine(file, string)) {
				values.push_back(GetDBValueNodePtr(string));
				if (!values[values.size()-1]) {
					WriteConsole("ERROR: Failed to parse node array in " + value->name);
					exit(0);
				}
			}

			value->data = new tDBNodeTemp*[values.size()];
			value->arrayCount = values.size();
			auto arr = (tDBNodeTemp**)value->data;
			for (int j = 0; j < value->arrayCount; j++) {
				arr[j] = values[j];
			}
		} break;
		case DBVALUE_VECTOR2:
		case DBVALUE_VECTOR3:
		case DBVALUE_VECTOR4: {
			int valueCount = (type - DBVALUE_VECTOR2) + 2;

			std::vector<float> values;

			while (ReadDBArrayNextLine(file, string)) {
				if (!string.starts_with("{ ")) {
					WriteConsole("ERROR: Failed to find vector in " + string);
					return false;
				}

				auto arr = new float[valueCount];
				GetDBValueVector<float>(string, arr, valueCount);
				for (int i = 0; i < valueCount; i++) {
					values.push_back(arr[i]);
				}
				delete[] arr;
			}

			value->data = new float[values.size()];
			value->arrayCount = values.size();
			auto arr = (float *)value->data;
			for (int j = 0; j < value->arrayCount; j++) {
				arr[j] = values[j];
			}
			value->arrayCount /= valueCount;
		} break;
		default: {
			WriteConsole("ERROR: arrays not implemented yet for type " + std::to_string(type));
			return false;
		}
	}
	return true;
}

void ParseDBLine(tDBNodeTemp* node, const std::string& line, std::ifstream& file) {
	if (line.length() < 3) return;
	std::string tmp = line;
	while (tmp[0] == '\t') tmp.erase(tmp.begin());
	if (tmp.starts_with("//")) return;

	for (int i = 0; i < DBVALUE_MAX_COUNT; i++) {
		auto typeName = aValueTypeNames[i];
		if (!typeName) continue;
		if (!tmp.starts_with(typeName)) continue;

		tDBValueTemp value;
		value.type = i;

		tmp.erase(tmp.begin(), tmp.begin() + strlen(typeName));
		if (tmp[0] == '*' && i == DBVALUE_STRING) tmp.erase(tmp.begin());
		if (tmp[0] != ' ') {
			WriteConsole("ERROR: Failed to read line " + line + " for node " + node->name);
			exit(0);
		}
		tmp.erase(tmp.begin());

		auto arrayBegin = tmp.find('[');
		auto valueStringLength = tmp.find(" = ");
		auto lengthToValue = valueStringLength + 3;
		if (valueStringLength == std::string::npos || valueStringLength < 1 || (arrayBegin != std::string::npos && arrayBegin > valueStringLength)) {
			WriteConsole("ERROR: Failed to read variable name " + line + " for node " + node->name);
			exit(0);
		}

		// todo - add fixed size string support
		//if (tmp[arrayBegin+1] != ']') {
		//
		//}

		bool isArray = arrayBegin != std::string::npos;
		if (isArray) valueStringLength = arrayBegin;
		else if (!tmp.ends_with(';')) {
			WriteConsole("ERROR: Failed to find line ending " + line + " for node " + node->name);
			exit(0);
		}
		else {
			// remove trailing semicolon
			tmp.pop_back();
		}

		// copy name string in
		value.name = GetSectionOfString(tmp, 0, valueStringLength);

		tmp.erase(tmp.begin(), tmp.begin() + lengthToValue);

		if (i == DBVALUE_STRING) {
			if (tmp[0] != '"') {
				WriteConsole("ERROR: Failed to read string " + line + " for node " + node->name);
				exit(0);
			}
			tmp.erase(tmp.begin());
			auto stringLength = tmp.find('"');
			if (stringLength == std::string::npos) {
				WriteConsole("ERROR: Failed to read end of string " + line + " for node " + node->name);
				exit(0);
			}

			value.data = new char[stringLength + 1];
			strcpy_s((char*)value.data, stringLength + 1, GetSectionOfString(tmp, 0, stringLength).c_str());
			value.arrayCount = stringLength + 1;
		}
		else {
			if (isArray) {
				if (!ReadDBArrayValue(&value, i, file, tmp)) {
					WriteConsole("ERROR: Parsing failed on line " + line);
					exit(0);
				}
			}
			else {
				value.arrayCount = 1;
				if (!ReadSingleDBValue(&value, i, tmp)) {
					WriteConsole("ERROR: Parsing failed on line " + line);
					exit(0);
				}
			}
		}

		node->values.push_back(value);
		return;
	}

	WriteConsole("ERROR: Failed to find a typename in " + line + " for node " + node->name);
	exit(0);
}

bool hasRootNode = false;
void ParseDBNode(const std::filesystem::directory_entry& at, bool readFiles) {
	const auto& path = at.path();
	auto pathWithoutExtension = path;
	if (!at.is_directory()) pathWithoutExtension.replace_extension("");

	bool isRootNode = path.filename() == "root";
	if (!readFiles) {
		if (!hasRootNode && isRootNode) hasRootNode = true;
		else {
			if (!hasRootNode && !isRootNode) {
				WriteConsole("ERROR: Root node not found");
				WriteConsole(path.filename().string());
				exit(0);
			}
			if (hasRootNode && isRootNode) {
				WriteConsole("ERROR: Root node found where it shouldn't be");
				exit(0);
			}
		}
	}

	if (isRootNode) {
		auto node = GetNodeForPath(pathWithoutExtension, true);
		node->parentNodeId = 0;
	}
	else {
		GenerateNodesToGetToRoot(pathWithoutExtension);

		auto node = GetNodeForPath(pathWithoutExtension, true);
		while (node != &aNodes[node->parentNodeId]) {
			node = &aNodes[node->parentNodeId];
		}
	}

	if (at.is_directory()) {
		for (const auto &entry: std::filesystem::directory_iterator(at)) {
			ParseDBNode(entry, readFiles);
		}
	}
	else if (path.extension() == ".h" && readFiles) {
		std::ifstream fin(path);
		if (!fin.is_open()) return;

		auto node = GetNodeForPath(pathWithoutExtension, false);
		if (!node) {
			WriteConsole("ERROR: Failed to find node " + pathWithoutExtension.string());
			exit(0);
		}
		for (std::string line; std::getline(fin, line); ) {
			ParseDBLine(node, line, fin);
		}
	}
}

tDBNodeTemp* GetPrevNodeWithParent(int id, int parentId) {
	auto i = id - 1;
	while (i > 0) {
		if (aNodes[i].parentNodeId == parentId) return &aNodes[i];
		i--;
	}
	return &aNodes[id];
}

tDBNodeTemp* GetLastNodeWithDirectParent(int id) {
	tDBNodeTemp* node = nullptr;
	auto i = id + 1;
	while (i < aNodes.size()) {
		if (aNodes[i].parentNodeId == id) node = &aNodes[i];
		i++;
	}
	return node;
}

bool WriteDB(const std::string& fileName) {
	dbBaseFolderPath = fileName + " extracted";
	if (!std::filesystem::is_directory(dbBaseFolderPath)) return false;

	std::ofstream fout(fileName, std::ios::out | std::ios::binary );
	if (!fout.is_open()) return false;

	gHeader.identifier = 0x1A424450;
	gHeader.version = 512;

	// read the structure first, then read data
	for (const auto& entry : std::filesystem::directory_iterator(dbBaseFolderPath)) {
		ParseDBNode(entry, false);
	}
	for (const auto& entry : std::filesystem::directory_iterator(dbBaseFolderPath)) {
		ParseDBNode(entry, true);
	}

	gHeader.numNodes = aNodes.size();
	fout.write((char*)&gHeader, sizeof(gHeader));

	// write nodes
	for (auto& node : aNodes) {
		node.baseFilePosition = fout.tellp();

		tDBNode nodeOut;
		nodeOut.dataCount = node.values.size();
		nodeOut.pNameString = (uint32_t)node.name.c_str();
		if (!node.values.empty()) nodeOut.pValues = (uint32_t)&node.values[0];
		int myId = &node - &aNodes[0];
		nodeOut.parentOffset = node.parentNodeId - myId;
		nodeOut.prevNodeOffset = (GetPrevNodeWithParent(myId, node.parentNodeId) - &aNodes[0]) - myId;
		if (auto lastNodeWithParent = GetLastNodeWithDirectParent(myId)) {
			nodeOut.lastChildOffset = (lastNodeWithParent - &node);
		}
		else nodeOut.lastChildOffset = 0;
		fout.write((char*)&nodeOut, sizeof(tDBNode));
	}

	for (auto& node : aNodes) {
		if (!node.values.empty()) {
			node.valuesFilePosition = fout.tellp();
			for (auto &value: node.values) {
				value.baseFilePosition = fout.tellp();

				// write value header
				tDBValue valueOut;
				valueOut.valueType = value.type;
				valueOut.size = value.arrayCount * GetDBValueTypeSize(value.type);
				valueOut.arrayType = value.arrayCount > 1 ? 1 : 0;
				if (value.type == DBVALUE_STRING) valueOut.arrayType = 2; // strings are always variable length arrays for now
				fout.write((char *) &valueOut, sizeof(tDBValue));

				// gather ids for each node from the pointer list
				if (value.type == DBVALUE_NODE) {
					auto data = (tDBNodeTemp **) value.data;
					for (int i = 0; i < value.arrayCount; i++) {
						int id = data[i] - &aNodes[0];
						fout.write((char *) &id, 2);
					}
				} else {
					// write variable length data
					fout.write((char *) value.data, valueOut.size);
				}
			}
		}

		// write node name strings
		node.nameFilePosition = fout.tellp();
		fout.write(node.name.c_str(), node.name.length() + 1);
		for (auto& value : node.values) {
			value.nameFilePosition = fout.tellp();
			fout.write(value.name.c_str(), value.name.length() + 1);
		}
	}

	for (auto& node : aNodes) {
		fout.seekp(node.baseFilePosition + 0xC); // seek to name offset
		// write name offset
		auto offset = node.nameFilePosition - node.baseFilePosition;
		fout.write((char*)&offset, sizeof(offset));

		// write values offset
		offset = node.valuesFilePosition - node.baseFilePosition;
		if (!node.valuesFilePosition) offset = 0;
		fout.write((char*)&offset, sizeof(offset));

		for (auto& value : node.values) {
			fout.seekp(value.baseFilePosition);
			offset = value.nameFilePosition - value.baseFilePosition;
			fout.write((char*)&offset, sizeof(offset));
		}
	}

	return true;
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		WriteConsole("Usage: FlatOut2DBMaker_gcp.exe <filename>");
		return 0;
	}
	if (!WriteDB(argv[1])) {
		WriteConsole("Failed to make binary database " + (std::string)argv[1] + "!");
	}
	return 0;
}
