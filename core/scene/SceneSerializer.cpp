#include "core/scene/SceneSerializer.h"

#include <glm/glm.hpp>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>

namespace Engine {
namespace Scene {

namespace {

std::string Escape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char c : value) {
        if (c == '"' || c == '\\') {
            escaped.push_back('\\');
        }
        escaped.push_back(c);
    }
    return escaped;
}

void AppendVec3(std::ostringstream& out, const char* key, const glm::vec3& v) {
    out << '"' << key << "\":[" << v.x << ',' << v.y << ',' << v.z << ']';
}

size_t SkipWhitespace(const std::string& json, size_t index) {
    while (index < json.size() && std::isspace(static_cast<unsigned char>(json[index]))) {
        ++index;
    }
    return index;
}

bool ReadNumber(const std::string& json, size_t& index, double& out) {
    index = SkipWhitespace(json, index);
    const size_t start = index;
    while (index < json.size() &&
           (std::isdigit(static_cast<unsigned char>(json[index])) || json[index] == '-' ||
            json[index] == '+' || json[index] == '.' || json[index] == 'e' || json[index] == 'E')) {
        ++index;
    }
    if (start == index) {
        return false;
    }
    out = std::stod(json.substr(start, index - start));
    return true;
}

bool ReadQuotedString(const std::string& json, size_t& index, std::string& out) {
    index = SkipWhitespace(json, index);
    if (index >= json.size() || json[index] != '"') {
        return false;
    }
    ++index;
    out.clear();
    while (index < json.size()) {
        const char c = json[index++];
        if (c == '\\' && index < json.size()) {
            out.push_back(json[index++]);
            continue;
        }
        if (c == '"') {
            return true;
        }
        out.push_back(c);
    }
    return false;
}

bool ReadVec3Array(const std::string& json, size_t& index, glm::vec3& out) {
    index = SkipWhitespace(json, index);
    if (index >= json.size() || json[index] != '[') {
        return false;
    }
    ++index;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    if (!ReadNumber(json, index, x)) {
        return false;
    }
    index = SkipWhitespace(json, index);
    if (index >= json.size() || json[index] != ',') {
        return false;
    }
    ++index;
    if (!ReadNumber(json, index, y)) {
        return false;
    }
    index = SkipWhitespace(json, index);
    if (index >= json.size() || json[index] != ',') {
        return false;
    }
    ++index;
    if (!ReadNumber(json, index, z)) {
        return false;
    }
    index = SkipWhitespace(json, index);
    if (index >= json.size() || json[index] != ']') {
        return false;
    }
    ++index;
    out = glm::vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    return true;
}

bool ExpectChar(const std::string& json, size_t& index, char expected) {
    index = SkipWhitespace(json, index);
    if (index >= json.size() || json[index] != expected) {
        return false;
    }
    ++index;
    return true;
}

bool ReadBool(const std::string& json, size_t& index, bool& out) {
    index = SkipWhitespace(json, index);
    if (json.compare(index, 4, "true") == 0) {
        out = true;
        index += 4;
        return true;
    }
    if (json.compare(index, 5, "false") == 0) {
        out = false;
        index += 5;
        return true;
    }
    return false;
}

bool ParseSceneNodeObject(const std::string& json, size_t& index, SceneNode& outNode) {
    if (!ExpectChar(json, index, '{')) {
        return false;
    }

    while (true) {
        index = SkipWhitespace(json, index);
        if (index < json.size() && json[index] == '}') {
            ++index;
            return true;
        }

        std::string key;
        if (!ReadQuotedString(json, index, key) || !ExpectChar(json, index, ':')) {
            return false;
        }

        if (key == "id") {
            double value = 0.0;
            if (!ReadNumber(json, index, value)) {
                return false;
            }
            outNode.id = static_cast<SceneNodeId>(value);
        } else if (key == "parent") {
            double value = 0.0;
            if (!ReadNumber(json, index, value)) {
                return false;
            }
            outNode.parentId = static_cast<SceneNodeId>(value);
        } else if (key == "name") {
            if (!ReadQuotedString(json, index, outNode.name)) {
                return false;
            }
        } else if (key == "position") {
            if (!ReadVec3Array(json, index, outNode.localPosition)) {
                return false;
            }
        } else if (key == "rotation") {
            if (!ReadVec3Array(json, index, outNode.localRotationDegrees)) {
                return false;
            }
        } else if (key == "scale") {
            if (!ReadVec3Array(json, index, outNode.localScale)) {
                return false;
            }
        } else if (key == "entity") {
            double value = 0.0;
            if (!ReadNumber(json, index, value)) {
                return false;
            }
            outNode.entity = static_cast<EntityID>(value);
        } else if (key == "active") {
            if (!ReadBool(json, index, outNode.active)) {
                return false;
            }
        } else {
            return false;
        }

        index = SkipWhitespace(json, index);
        if (index < json.size() && json[index] == ',') {
            ++index;
            continue;
        }
        if (index < json.size() && json[index] == '}') {
            ++index;
            return true;
        }
        return false;
    }
}

bool ParsePrefabNodeObject(const std::string& json, size_t& index, SceneNode& outNode) {
    if (!ExpectChar(json, index, '{')) {
        return false;
    }

    while (true) {
        index = SkipWhitespace(json, index);
        if (index < json.size() && json[index] == '}') {
            ++index;
            return true;
        }

        std::string key;
        if (!ReadQuotedString(json, index, key) || !ExpectChar(json, index, ':')) {
            return false;
        }

        if (key == "name") {
            if (!ReadQuotedString(json, index, outNode.name)) {
                return false;
            }
        } else if (key == "position") {
            if (!ReadVec3Array(json, index, outNode.localPosition)) {
                return false;
            }
        } else if (key == "rotation") {
            if (!ReadVec3Array(json, index, outNode.localRotationDegrees)) {
                return false;
            }
        } else if (key == "scale") {
            if (!ReadVec3Array(json, index, outNode.localScale)) {
                return false;
            }
        } else {
            return false;
        }

        index = SkipWhitespace(json, index);
        if (index < json.size() && json[index] == ',') {
            ++index;
            continue;
        }
        if (index < json.size() && json[index] == '}') {
            ++index;
            return true;
        }
        return false;
    }
}

} // namespace

std::string SceneSerializer::SerializeScene(const Scene& scene) {
    std::ostringstream out;
    out << "{\"nodes\":[";
    bool first = true;
    for (const auto& entry : scene.Nodes()) {
        const SceneNode& node = entry.second;
        if (!first) {
            out << ',';
        }
        first = false;
        out << '{';
        out << "\"id\":" << node.id << ',';
        out << "\"parent\":" << node.parentId << ',';
        out << "\"name\":\"" << Escape(node.name) << "\",";
        AppendVec3(out, "position", node.localPosition);
        out << ',';
        AppendVec3(out, "rotation", node.localRotationDegrees);
        out << ',';
        AppendVec3(out, "scale", node.localScale);
        out << ",\"entity\":" << node.entity;
        out << ",\"active\":" << (node.active ? "true" : "false");
        out << '}';
    }
    out << "]}";
    return out.str();
}

bool SceneSerializer::DeserializeScene(const std::string& json, Scene& outScene) {
    outScene.Clear();

    const size_t nodesKey = json.find("\"nodes\"");
    if (nodesKey == std::string::npos) {
        return false;
    }

    size_t index = json.find('[', nodesKey);
    if (index == std::string::npos) {
        return false;
    }
    ++index;

    SceneNodeId maxId = 0;
    SceneNodeId rootId = kInvalidSceneNode;
    std::vector<SceneNode> parsedNodes;

    index = SkipWhitespace(json, index);
    if (index < json.size() && json[index] == ']') {
        return true;
    }

    while (index < json.size()) {
        SceneNode node;
        if (!ParseSceneNodeObject(json, index, node)) {
            return false;
        }
        parsedNodes.push_back(node);
        maxId = std::max(maxId, node.id);
        if (node.parentId == kInvalidSceneNode) {
            rootId = node.id;
        }

        index = SkipWhitespace(json, index);
        if (index < json.size() && json[index] == ',') {
            ++index;
            continue;
        }
        if (index < json.size() && json[index] == ']') {
            ++index;
            break;
        }
        return false;
    }

    outScene.m_nodes.clear();
    for (const SceneNode& node : parsedNodes) {
        outScene.m_nodes[node.id] = node;
    }

    for (auto& entry : outScene.m_nodes) {
        entry.second.children.clear();
    }
    for (auto& entry : outScene.m_nodes) {
        if (entry.second.parentId != kInvalidSceneNode) {
            if (SceneNode* parent = outScene.GetNode(entry.second.parentId)) {
                parent->children.push_back(entry.first);
            }
        }
    }

    outScene.m_rootId = rootId;
    outScene.m_nextId = maxId + 1;
    return true;
}

std::string SceneSerializer::SerializePrefab(const Prefab& prefab) {
    std::ostringstream out;
    out << "{\"name\":\"" << Escape(prefab.name) << "\",\"root\":" << prefab.rootNodeIndex << ",\"nodes\":[";
    for (size_t i = 0; i < prefab.nodes.size(); ++i) {
        if (i > 0) {
            out << ',';
        }
        const SceneNode& node = prefab.nodes[i];
        out << '{';
        out << "\"name\":\"" << Escape(node.name) << "\",";
        AppendVec3(out, "position", node.localPosition);
        out << ',';
        AppendVec3(out, "rotation", node.localRotationDegrees);
        out << ',';
        AppendVec3(out, "scale", node.localScale);
        out << '}';
    }
    out << "]}";
    return out.str();
}

bool SceneSerializer::DeserializePrefab(const std::string& json, Prefab& outPrefab) {
    outPrefab = Prefab{};
    size_t index = 0;
    index = SkipWhitespace(json, index);
    if (!ExpectChar(json, index, '{')) {
        return false;
    }

    while (true) {
        index = SkipWhitespace(json, index);
        if (index < json.size() && json[index] == '}') {
            ++index;
            return !outPrefab.nodes.empty();
        }

        std::string key;
        if (!ReadQuotedString(json, index, key) || !ExpectChar(json, index, ':')) {
            return false;
        }

        if (key == "name") {
            if (!ReadQuotedString(json, index, outPrefab.name)) {
                return false;
            }
        } else if (key == "root") {
            double value = 0.0;
            if (!ReadNumber(json, index, value)) {
                return false;
            }
            outPrefab.rootNodeIndex = static_cast<SceneNodeId>(value);
        } else if (key == "nodes") {
            if (!ExpectChar(json, index, '[')) {
                return false;
            }
            while (true) {
                index = SkipWhitespace(json, index);
                if (index < json.size() && json[index] == ']') {
                    ++index;
                    break;
                }
                SceneNode node;
                if (!ParsePrefabNodeObject(json, index, node)) {
                    return false;
                }
                outPrefab.nodes.push_back(node);
                index = SkipWhitespace(json, index);
                if (index < json.size() && json[index] == ',') {
                    ++index;
                    continue;
                }
                if (index < json.size() && json[index] == ']') {
                    ++index;
                    break;
                }
                return false;
            }
        } else {
            return false;
        }

        index = SkipWhitespace(json, index);
        if (index < json.size() && json[index] == ',') {
            ++index;
            continue;
        }
        if (index < json.size() && json[index] == '}') {
            ++index;
            return !outPrefab.nodes.empty();
        }
        return false;
    }
}

} // namespace Scene
} // namespace Engine
