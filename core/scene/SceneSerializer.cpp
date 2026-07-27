#include "core/scene/SceneSerializer.h"

#include <glm/glm.hpp>
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
    (void)json;
    (void)outScene;
    // Minimal parser stub — extend with a real JSON parser when scenes grow.
    return false;
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
    (void)json;
    (void)outPrefab;
    return false;
}

} // namespace Scene
} // namespace Engine
