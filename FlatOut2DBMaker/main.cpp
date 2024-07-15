#include <vector>
#include <cstring>
#include "../shared.h"

struct __attribute__((packed, aligned(1))) tDBValue {
	uint32_t pNameString;	// +0
	uint8_t valueType;		// +4
	uint16_t size;			// +5 array size included
	uint8_t arrayType;		// +7
	uint32_t dataPtr;		// +8 nulled by the game's reader, only used at runtime
	char data[0];			// +C
};

struct tDBNode {
	uint32_t vtable;			// +0
	int16_t parentOffset;		// +4
	int16_t nextNodeOffset;		// +6 amount of nodes to get to the next one in the folder
	int16_t prevNodeOffset;		// +8 usually -1, 0 if it's the first one, amount of nodes to get to the previous one in the folder
	uint16_t dataCount;			// +A
	uint32_t pNameString;		// +C
	uint32_t pValues;			// +10
};

tDBHeader gHeader;

struct tDBValueTemp {
	std::string name;
	int type = 0;
	int arrayCount = 0;
	void* data = nullptr;
};

struct tDBNodeTemp {
	std::filesystem::path fullPath;
	std::string name;
	std::vector<tDBValueTemp> values;
	int parentNodeId = 0;
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

void ParseDBLine(tDBNodeTemp* node, const std::string& line, std::ifstream& file) {
	if (line.length() < 3) return;
	std::string tmp = line;
	while (tmp[0] == '\t') tmp.erase(tmp.begin());

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
		//WriteConsole(value.name);

		tmp.erase(tmp.begin(), tmp.begin() + lengthToValue);

		if (IsDBTypeVector(i) && !tmp.starts_with("{ ")) {
			WriteConsole("ERROR: Failed to find vector in " + line + " for node " + node->name);
			exit(0);
		}

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
			value.arrayCount = stringLength;
		}
		else {
			if (isArray) {
				// todo add all array types
				if (i == DBVALUE_FLOAT) {
					if (!tmp.ends_with("{")) {
						WriteConsole("ERROR: Invalid array at " + line + " for node " + node->name);
						exit(0);
					}

					std::vector<float> values;

					// read next line
					std::getline(file, tmp);
					while (!tmp.ends_with("};")) {
						if (!tmp.starts_with("\t")) {
							WriteConsole("ERROR: Failed to read array at " + tmp + " for node " + node->name);
							exit(0);
						}
						tmp.erase(tmp.begin());
						values.push_back(std::stof(tmp));
						std::getline(file, tmp);
					}

					value.data = new float[values.size()];
					value.arrayCount = values.size();
					auto arr = (float *)value.data;
					for (int j = 0; j < value.arrayCount; j++) {
						arr[j] = values[j];
					}
				}
				else {
					WriteConsole("ERROR: arrays not implemented yet for type " + std::to_string(i));
					exit(0);
				}
			}
			else {
				value.arrayCount = 1;
				if (i == DBVALUE_INT) {
					value.data = new int;
					*(int *) value.data = std::stoi(tmp);
				} else if (i == DBVALUE_FLOAT) {
					value.data = new float;
					*(float *) value.data = std::stof(tmp);
				} else if (i == DBVALUE_BOOL) {
					value.data = new bool;
					*(bool *) value.data = tmp.starts_with("true");
				} else if (i == DBVALUE_NODE) {
					value.data = new tDBNodeTemp *;
					if (!tmp.starts_with("\"")) {
						WriteConsole(
								"ERROR: Failed to read start of pointer string " + line + " for node " + node->name);
						exit(0);
					}
					tmp.erase(tmp.begin());
					if (!tmp.ends_with("\"")) {
						WriteConsole("ERROR: Failed to read end of pointer string " + line + " for node " + node->name);
						exit(0);
					}
					tmp.pop_back();

					auto pNode = GetNodeForPath(dbBaseFolderPath.string() + "/" + tmp, false);
					if (!pNode) {
						WriteConsole("ERROR: Failed to find node pointed at by " + line + " for node " + node->name);
						WriteConsole(dbBaseFolderPath.string() + "/" + tmp);
						exit(0);
					}
					value.data = pNode;
					value.arrayCount = 1;
				} else if (IsDBTypeVector(i)) {
					int valueCount = (i - DBVALUE_VECTOR2) + 2;

					tmp.erase(tmp.begin(), tmp.begin() + 2);

					value.data = new float[valueCount];
					auto arr = (float *) value.data;
					for (int j = 0; j < valueCount; j++) {
						arr[j] = std::stof(tmp);

						// find next value
						if (j + 1 < valueCount) {
							auto next = tmp.find(", ");
							if (next == std::string::npos) {
								WriteConsole("ERROR: Failed to read vector in " + line + " for node " + node->name);
								exit(0);
							}
							tmp.erase(tmp.begin(), tmp.begin() + next + 2);
						}
					}
				} else if (i == DBVALUE_RGBA) {
					int valueCount = 4;

					tmp.erase(tmp.begin(), tmp.begin() + 2);

					value.data = new uint8_t[valueCount];
					auto arr = (uint8_t *) value.data;
					for (int j = 0; j < valueCount; j++) {
						arr[j] = std::stoi(tmp);

						// find next value
						if (j + 1 < valueCount) {
							auto next = tmp.find(", ");
							if (next == std::string::npos) {
								WriteConsole("ERROR: Failed to read RGBA in " + line + " for node " + node->name);
								exit(0);
							}
							tmp.erase(tmp.begin(), tmp.begin() + next + 2);
						}
					}
				} else {
					WriteConsole("ERROR: type not implemented: " + std::to_string(i));
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
	auto path = at.path();
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
		}
		for (std::string line; std::getline(fin, line); ) {
			ParseDBLine(node, line, fin);
		}
	}
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
