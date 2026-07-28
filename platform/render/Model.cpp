#include "render/Model.h"
#include "render/MeshAssetResolve.h"
#include "render/Raycast.h"
#include "Logger.h"
#include "stb_image.h"
#include <assimp/config.h>
#include <cstdio>
#include <algorithm>
#include <stdexcept>
#include <limits>
#include <map>
#include <string>
#include <vector>
#include <filesystem>
#include <glm/glm.hpp>
#include <cmath>
#include <cctype>
#include <cstdlib>

namespace {

namespace fs = std::filesystem;

std::string ToLowerAscii(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

bool LooksLikeNonDiffuseMap(const std::string& lower) {
    return lower.find("norm") != std::string::npos ||
           lower.find("bump") != std::string::npos ||
           lower.find("spec") != std::string::npos ||
           lower.find("rough") != std::string::npos ||
           lower.find("metal") != std::string::npos ||
           lower.find("opacity") != std::string::npos ||
           lower.find("mask") != std::string::npos ||
           lower.find("cavity") != std::string::npos ||
           lower.find("displ") != std::string::npos;
}

bool LooksLikeDiffuseFilename(const std::string& lower) {
    if (LooksLikeNonDiffuseMap(lower)) {
        return false;
    }
    return lower.find("dif") != std::string::npos || lower.find("diff") != std::string::npos ||
           lower.find("albedo") != std::string::npos || lower.find("color") != std::string::npos ||
           lower.find("basecolor") != std::string::npos || lower.find("_col") != std::string::npos;
}

bool IsProbablyRasterTextureFile(const fs::path& fp) {
    std::string e = ToLowerAscii(fp.extension().string());
    return e == ".jpg" || e == ".jpeg" || e == ".png" || e == ".bmp" || e == ".tga" ||
           e == ".gif" || e == ".webp" || e == ".hdr" || e == ".tif" || e == ".tiff";
}

// Lower rank tries first when several diffuse-looking maps exist on disk (prefer 2k over 8k etc.).
int DiffuseNameLodTier(const std::string& lower) {
    if (lower.find("_8k") != std::string::npos) return 80;
    if (lower.find("_4k") != std::string::npos) return 60;
    if (lower.find("_2k") != std::string::npos) return 40;
    if (lower.find("_1k") != std::string::npos) return 30;
    if (lower.find("_512") != std::string::npos) return 20;
    return 0;
}

void AppendDiffuseTexturePathsFromFolder(const fs::path& folder, std::vector<fs::path>& out,
                                         std::map<fs::path, bool>& dedupSeen) {
    std::error_code ec;
    if (!fs::is_directory(folder, ec)) {
        return;
    }
    for (const auto& ent : fs::directory_iterator(folder, fs::directory_options::skip_permission_denied, ec)) {
        ec.clear();
        if (!ent.is_regular_file(ec)) {
            continue;
        }
        const fs::path& fp = ent.path();
        if (!IsProbablyRasterTextureFile(fp)) {
            continue;
        }
        const std::string lowerName = ToLowerAscii(fp.filename().string());
        if (!LooksLikeDiffuseFilename(lowerName)) {
            continue;
        }
        fs::path norm = fp.lexically_normal();
        if (!dedupSeen[norm]) {
            dedupSeen[norm] = true;
            out.push_back(std::move(norm));
        }
    }
}

} // namespace

namespace Engine {
namespace Platform {
namespace Render {

Model::Model() {
    m_textureBaseDir = "";
    m_modelDirectory = "";
    m_minX = m_minY = m_minZ = std::numeric_limits<float>::max();
    m_maxX = m_maxY = m_maxZ = std::numeric_limits<float>::lowest();
    m_boundsCalculated = false;
}

void Model::LoadModel(const std::string& fileName) {
    const std::string resolved = ResolveMeshLoadPath(fileName);
    const std::string pathToRead = resolved.empty() ? fileName : resolved;
    if (!resolved.empty() && resolved != fileName) {
        LOG_INFO_FMT("Model", "Resolved mesh path \"%s\" -> \"%s\"", fileName.c_str(), resolved.c_str());
    }

    Assimp::Importer importer;
    importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
    const aiScene* scene = importer.ReadFile(
        pathToRead,
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_GenSmoothNormals |
        aiProcess_JoinIdenticalVertices |
        aiProcess_LimitBoneWeights
    );

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::string errorMsg = std::string("Model (") + pathToRead + ") failed to load: " + importer.GetErrorString();
        LOG_ERROR_FMT("Model", "%s", errorMsg.c_str());
        throw std::runtime_error(errorMsg);
    }

    if (!LoadModelFromScene(scene, pathToRead)) {
        throw std::runtime_error("Model loaded but contains no meshes: " + pathToRead);
    }
}

bool Model::LoadModelFromScene(const aiScene* scene, const std::string& sourcePath) {
    if (!scene || !scene->mRootNode || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)) {
        LOG_ERROR_FMT("Model", "LoadModelFromScene failed: invalid scene (%s)", sourcePath.c_str());
        return false;
    }

    // Extract model directory for texture path resolution
    size_t lastSlash = sourcePath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        m_modelDirectory = sourcePath.substr(0, lastSlash + 1);
    } else {
        m_modelDirectory = "";
    }

    // Reset bounds before loading
    m_minX = m_minY = m_minZ = std::numeric_limits<float>::max();
    m_maxX = m_maxY = m_maxZ = std::numeric_limits<float>::lowest();
    m_boundsCalculated = false;

    LoadNode(scene->mRootNode, scene);
    LoadMaterials(scene);

    if (m_meshList.empty()) {
        LOG_ERROR_FMT("Model", "LoadModelFromScene: no meshes in %s", sourcePath.c_str());
        return false;
    }

    CalculateBounds(scene);
    LOG_INFO_FMT("Model", "Model loaded: %zu meshes, height: %.2f", m_meshList.size(), GetHeight());
    return true;
}

void Model::LoadNode(aiNode* node, const aiScene* scene) {
    // Load all meshes in this node
    for (size_t i = 0; i < node->mNumMeshes; i++) {
        LoadMesh(scene->mMeshes[node->mMeshes[i]], scene);
    }
    
    // Recursively load child nodes
    for (size_t i = 0; i < node->mNumChildren; i++) {
        LoadNode(node->mChildren[i], scene);
    }
}

void Model::LoadMesh(aiMesh* mesh, const aiScene* /*scene*/) {
    std::vector<GLfloat> vertices;
    std::vector<unsigned int> indices;
    
    // Prepare bone data structures
    std::vector<GLint> boneIDs;      // Bone IDs per vertex (up to 4 per vertex)
    std::vector<GLfloat> boneWeights; // Bone weights per vertex (up to 4 per vertex)
    bool hasBones = mesh->HasBones();
    
    if (hasBones) {
        LOG_INFO_FMT("Model", "Extracting bone data: %u bones, %u vertices", mesh->mNumBones, mesh->mNumVertices);
        boneIDs.resize(mesh->mNumVertices * 4, 0);
        boneWeights.resize(mesh->mNumVertices * 4, 0.0f);
        
        if (m_boneNames.empty()) {
            m_boneNames.resize(mesh->mNumBones);
            for (unsigned int i = 0; i < mesh->mNumBones; i++) {
                m_boneNames[i] = std::string(mesh->mBones[i]->mName.C_Str());
            }
        }
        
        std::map<std::string, unsigned int> boneNameToIndex;
        for (unsigned int i = 0; i < mesh->mNumBones; i++) {
            boneNameToIndex[std::string(mesh->mBones[i]->mName.C_Str())] = i;
        }
        
        for (unsigned int boneIdx = 0; boneIdx < mesh->mNumBones; boneIdx++) {
            const aiBone* bone = mesh->mBones[boneIdx];
            unsigned int boneIndex = boneIdx;
            
            for (unsigned int weightIdx = 0; weightIdx < bone->mNumWeights; weightIdx++) {
                const aiVertexWeight& weight = bone->mWeights[weightIdx];
                unsigned int vertexID = weight.mVertexId;
                if (vertexID >= mesh->mNumVertices) continue; // Safety: skip invalid vertex refs
                float vertexWeight = weight.mWeight;
                
                for (int slot = 0; slot < 4; slot++) {
                    if (boneWeights[vertexID * 4 + slot] == 0.0f) {
                        boneIDs[vertexID * 4 + slot] = static_cast<GLint>(boneIndex);
                        boneWeights[vertexID * 4 + slot] = vertexWeight;
                        break;
                    }
                }
            }
        }
    }
    
    // Load vertex data (position, texture coordinates, normals)
    for (size_t i = 0; i < mesh->mNumVertices; i++) {
        // Position
        vertices.insert(vertices.end(), {
            mesh->mVertices[i].x,
            mesh->mVertices[i].y,
            mesh->mVertices[i].z
        });
        
        // Texture coordinates
        if (mesh->mTextureCoords[0]) {
            vertices.insert(vertices.end(), {
                mesh->mTextureCoords[0][i].x,
                mesh->mTextureCoords[0][i].y
            });
        } else {
            vertices.insert(vertices.end(), { 0.0f, 0.0f });
        }
        
        // Normals (flipped for OpenGL coordinate system)
        vertices.insert(vertices.end(), {
            -mesh->mNormals[i].x,
            -mesh->mNormals[i].y,
            -mesh->mNormals[i].z
        });
    }
    
    // Load indices
    for (size_t i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (size_t j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }
    
    // Store vertex positions for color application
    std::vector<glm::vec3> meshPositions;
    meshPositions.reserve(mesh->mNumVertices);
    for (size_t i = 0; i < mesh->mNumVertices; i++) {
        meshPositions.push_back(glm::vec3(
            mesh->mVertices[i].x,
            mesh->mVertices[i].y,
            mesh->mVertices[i].z
        ));
    }
    m_vertexPositions.push_back(meshPositions);
    
    // Create mesh with bone data if available
    // numOfVertices = vertex count (vertices has 8 floats per vertex)
    Mesh* newMesh = new Mesh();
    unsigned int vertexCount = static_cast<unsigned int>(mesh->mNumVertices);
    if (hasBones && !boneIDs.empty()) {
        newMesh->CreateMeshWithBones(&vertices[0], &indices[0],
                                    vertexCount,
                                    static_cast<unsigned int>(indices.size()),
                                    &boneIDs[0], &boneWeights[0],
                                    vertexCount);
    } else {
        newMesh->CreateMesh(&vertices[0], &indices[0], 
                           vertexCount, 
                           static_cast<unsigned int>(indices.size()));
    }
    AppendCollisionTriangles(&vertices[0], indices.data(), vertexCount,
                             static_cast<unsigned int>(indices.size()));
    m_meshList.push_back(newMesh);
    m_meshToTex.push_back(mesh->mMaterialIndex);
}

void Model::LoadMaterials(const aiScene* scene) {
    m_textureList.resize(scene->mNumMaterials);

    for (size_t i = 0; i < scene->mNumMaterials; i++) {
        aiMaterial* material = scene->mMaterials[i];
        m_textureList[i] = nullptr;

        auto tryLoadTexturePath = [&](const aiString& aiPath) -> bool {
            const char* raw = aiPath.C_Str();
            if (!raw || raw[0] == '\0') {
                return false;
            }
            const std::string pstr(raw);

            // Assimp embedded textures: "*0", "*1", ... index into scene->mTextures (compressed JPEG/PNG blob).
            if (pstr[0] == '*') {
                const int ti = std::atoi(pstr.c_str() + 1);
                if (ti < 0 || static_cast<unsigned int>(ti) >= scene->mNumTextures || !scene->mTextures) {
                    LOG_WARNING_FMT("Model", "Material %zu: invalid embedded texture ref '%s'", i, pstr.c_str());
                    return false;
                }
                const aiTexture* et = scene->mTextures[ti];
                if (!et || !et->pcData || et->mWidth == 0) {
                    LOG_WARNING_FMT("Model", "Material %zu: empty embedded texture '%s'", i, pstr.c_str());
                    return false;
                }
                if (et->mHeight == 0) {
                    auto* t = new Texture("");
                    const auto* bytes = reinterpret_cast<const unsigned char*>(et->pcData);
                    if (t->LoadFromMemory(bytes, static_cast<int>(et->mWidth))) {
                        m_textureList[i] = t;
                        return true;
                    }
                    delete t;
                    LOG_WARNING_FMT("Model", "Material %zu: stb failed embedded '%s' (hint=%.3s)", i, pstr.c_str(),
                                    et->achFormatHint);
                    return false;
                }
                LOG_WARNING_FMT("Model", "Material %zu: skipping raw embedded texture '%s' (uncompressed not supported)",
                                i, pstr.c_str());
                return false;
            }

            std::string texturePath = ResolveTexturePath(pstr);
            if (texturePath.empty() || texturePath[0] == '*' || texturePath[0] == '#') {
                return false;
            }

            auto* t = new Texture(texturePath.c_str());
            if (!t->LoadTextureA() && !t->LoadTexture()) {
                LOG_WARNING_FMT("Model", "Material %zu: failed loading '%s' (resolved: %s)", i, pstr.c_str(),
                                texturePath.c_str());
                delete t;
                return false;
            }
            m_textureList[i] = t;
            return true;
        };

        static const aiTextureType kAlbedoTypes[] = {
            aiTextureType_DIFFUSE,
            aiTextureType_BASE_COLOR,
        };

        for (aiTextureType albedoSlot : kAlbedoTypes) {
            if (material->GetTextureCount(albedoSlot) == 0) {
                continue;
            }
            aiString path;
            if (material->GetTexture(albedoSlot, 0, &path) != AI_SUCCESS) {
                continue;
            }
            if (tryLoadTexturePath(path)) {
                break;
            }
        }

        // FBX occasionally lists file-based maps under UNKNOWN; pick plausible albedo filenames.
        if (!m_textureList[i] && material->GetTextureCount(aiTextureType_UNKNOWN) > 0) {
            const unsigned maxU = std::min(material->GetTextureCount(aiTextureType_UNKNOWN), 16u);
            for (unsigned ui = 0; ui < maxU; ui++) {
                aiString path;
                if (material->GetTexture(aiTextureType_UNKNOWN, ui, &path) != AI_SUCCESS) {
                    continue;
                }
                const std::string lower = ToLowerAscii(std::string(path.C_Str()));
                if (!LooksLikeDiffuseFilename(lower)) {
                    continue;
                }
                if (tryLoadTexturePath(path)) {
                    break;
                }
            }
        }

        // Maps sometimes referenced only under ambient/emissive slots; still diffuse-like filenames.
        if (!m_textureList[i]) {
            static const aiTextureType kExtraProbeTypes[] = {aiTextureType_AMBIENT, aiTextureType_EMISSIVE};
            for (aiTextureType ty : kExtraProbeTypes) {
                if (material->GetTextureCount(ty) == 0) {
                    continue;
                }
                aiString path;
                if (material->GetTexture(ty, 0, &path) != AI_SUCCESS) {
                    continue;
                }
                const std::string lower = ToLowerAscii(std::string(path.C_Str()));
                if (!LooksLikeDiffuseFilename(lower)) {
                    continue;
                }
                if (tryLoadTexturePath(path)) {
                    break;
                }
            }
        }

        // Material lost file paths but FBX still carries compressed embedded textures (use *ti indices).
        if (!m_textureList[i] && scene->mNumTextures > 0 && scene->mTextures) {
            for (unsigned ti = 0; ti < scene->mNumTextures && !m_textureList[i]; ++ti) {
                const aiTexture* et = scene->mTextures[ti];
                if (!et || !et->pcData || et->mWidth == 0 || et->mHeight != 0) {
                    continue;
                }
                const std::string fn = ToLowerAscii(std::string(et->mFilename.C_Str()));
                const bool unnamed = fn.empty();
                if (scene->mNumTextures > 1) {
                    if (unnamed || !LooksLikeDiffuseFilename(fn)) {
                        continue;
                    }
                }
                char buf[32];
                std::snprintf(buf, sizeof(buf), "*%u", ti);
                aiString path;
                path.Set(buf);
                if (tryLoadTexturePath(path)) {
                    LOG_INFO_FMT("Model",
                                 "Material %zu: diffuse from embedded scene texture slot %u (hint=%.3s name='%s')", i,
                                 ti, et->achFormatHint, et->mFilename.C_Str() ? et->mFilename.C_Str() : "");
                    break;
                }
            }
        }

        // Unlinked materials: common layout is mesh FBX beside tex/*_dif_*.{jpg,png,...}
        if (!m_textureList[i] && !m_modelDirectory.empty()) {
            std::vector<std::filesystem::path> candidateFiles;
            std::map<std::filesystem::path, bool> seenPaths;
            const std::filesystem::path root(m_modelDirectory);
            AppendDiffuseTexturePathsFromFolder(root / "tex", candidateFiles, seenPaths);
            AppendDiffuseTexturePathsFromFolder(root / "Tex", candidateFiles, seenPaths);
            AppendDiffuseTexturePathsFromFolder(root / "textures", candidateFiles, seenPaths);
            AppendDiffuseTexturePathsFromFolder(root / "Textures", candidateFiles, seenPaths);
            AppendDiffuseTexturePathsFromFolder(root / "maps", candidateFiles, seenPaths);
            AppendDiffuseTexturePathsFromFolder(root / "Maps", candidateFiles, seenPaths);
            AppendDiffuseTexturePathsFromFolder(root, candidateFiles, seenPaths);

            std::sort(candidateFiles.begin(), candidateFiles.end(),
                      [](const std::filesystem::path& a, const std::filesystem::path& b) {
                const std::string la = ToLowerAscii(a.filename().string());
                const std::string lb = ToLowerAscii(b.filename().string());
                const int ra = DiffuseNameLodTier(la);
                const int rb = DiffuseNameLodTier(lb);
                if (ra != rb) {
                    return ra < rb;
                }
                return la < lb;
            });

            for (const std::filesystem::path& cand : candidateFiles) {
                aiString aiFile;
                std::filesystem::path candPref(cand);
                aiFile.Set(candPref.make_preferred().string().c_str());
                if (tryLoadTexturePath(aiFile)) {
                    LOG_INFO_FMT("Model", "Material %zu: diffuse from filesystem fallback '%s'", i,
                                 cand.string().c_str());
                    break;
                }
            }
        }

        if (!m_textureList[i]) {
            aiColor4D diffuse(0.75f, 0.75f, 0.75f, 1.0f);
            if (material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS) {
                auto* solid = new Texture();
                const unsigned char r = static_cast<unsigned char>(std::clamp(diffuse.r, 0.0f, 1.0f) * 255.0f);
                const unsigned char g = static_cast<unsigned char>(std::clamp(diffuse.g, 0.0f, 1.0f) * 255.0f);
                const unsigned char b = static_cast<unsigned char>(std::clamp(diffuse.b, 0.0f, 1.0f) * 255.0f);
                const unsigned char a = static_cast<unsigned char>(std::clamp(diffuse.a, 0.0f, 1.0f) * 255.0f);
                if (solid->LoadSolidColor(r, g, b, a)) {
                    m_textureList[i] = solid;
                    LOG_INFO_FMT("Model",
                                 "Material %zu: using solid diffuse fallback (%.2f, %.2f, %.2f)",
                                 i, diffuse.r, diffuse.g, diffuse.b);
                } else {
                    delete solid;
                }
            }
        }

        if (!m_textureList[i]) {
            aiString matName;
            if (material->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS && matName.C_Str() && matName.C_Str()[0] != '\0') {
                LOG_INFO_FMT("Model", "Material %zu '%s': no diffuse/base-color texture resolved", i, matName.C_Str());
            }
        }
    }
}

std::string Model::ResolveTexturePath(const std::string& texturePath) {
    if (texturePath.empty()) {
        return texturePath;
    }
    // Embedded or procedural textures (not files on disk)
    if (texturePath[0] == '*' || texturePath[0] == '#') {
        return texturePath;
    }

    namespace fs = std::filesystem;

    auto fileExists = [](const fs::path& p) -> bool {
        std::error_code ec;
        return fs::is_regular_file(p, ec);
    };

    fs::path rel(texturePath);
    const fs::path filenameOnly = rel.filename();

    auto pushCandidates = [&](const fs::path& baseRaw, std::vector<fs::path>& out) {
        if (baseRaw.empty()) {
            return;
        }
        fs::path base = baseRaw;
        std::string bs = base.string();
        while (!bs.empty() && (bs.back() == '/' || bs.back() == '\\')) {
            bs.pop_back();
        }
        base = fs::path(bs);

        auto pushUnique = [&](const fs::path& cand) {
            fs::path n = cand.lexically_normal();
            if (std::find(out.begin(), out.end(), n) == out.end()) {
                out.push_back(std::move(n));
            }
        };

        pushUnique(base / rel);
        pushUnique(base / filenameOnly);

        pushUnique(base / "tex" / filenameOnly);
        pushUnique(base / "Tex" / filenameOnly);
        pushUnique(base / "textures" / filenameOnly);
        pushUnique(base / "Textures" / filenameOnly);
        pushUnique(base / "maps" / filenameOnly);
        pushUnique(base / "Maps" / filenameOnly);
    };

    std::vector<fs::path> candidates;
    candidates.reserve(24);

    if (!m_textureBaseDir.empty()) {
        pushCandidates(fs::path(m_textureBaseDir), candidates);
    }
    if (!m_modelDirectory.empty()) {
        pushCandidates(fs::path(m_modelDirectory), candidates);
    }

    for (const fs::path& cand : candidates) {
        if (fileExists(cand)) {
            fs::path out = cand;
            return out.make_preferred().string();
        }
    }

    auto lowerExt = [](std::string e) -> std::string {
        std::transform(e.begin(), e.end(), e.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return e;
    };

    for (const fs::path& cand : candidates) {
        std::string ext = lowerExt(cand.extension().string());
        if (ext != ".tif" && ext != ".tiff") {
            continue;
        }
        const fs::path parent = cand.parent_path();
        const fs::path stem = cand.stem();
        static const char* const kFallbackExt[] = {".jpg", ".jpeg", ".png"};
        for (const char* alt : kFallbackExt) {
            const fs::path altPath = parent / (stem.string() + alt);
            if (fileExists(altPath)) {
                fs::path altOut = altPath;
                return altOut.make_preferred().string();
            }
        }
    }

    // Legacy resolution: flattened filename beside model/base (prior behavior clients may rely on)
    if (!m_textureBaseDir.empty()) {
        std::string basePath = m_textureBaseDir;
        if (basePath.back() != '/' && basePath.back() != '\\') {
            basePath += "/";
        }
        return basePath + filenameOnly.string();
    }

    if (!m_modelDirectory.empty()) {
        return m_modelDirectory + filenameOnly.string();
    }

    return filenameOnly.empty() ? texturePath : filenameOnly.string();
}

void Model::RenderModel() {
    if (m_meshList.empty()) {
        return;
    }
    
    for (size_t i = 0; i < m_meshList.size(); i++) {
        if (!m_meshList[i]) {
            continue;
        }
        
        unsigned int materialIndex = m_meshToTex[i];
        
        if (materialIndex < m_textureList.size() && m_textureList[materialIndex]) {
            m_textureList[materialIndex]->UseTexture();
        }
        
        m_meshList[i]->RenderMesh();
    }
}

void Model::RenderModelPerMeshTexture(GLint useTextureUniformLoc) {
    if (m_meshList.empty()) {
        return;
    }
    
    for (size_t i = 0; i < m_meshList.size(); i++) {
        if (!m_meshList[i]) {
            continue;
        }
        
        unsigned int materialIndex = m_meshToTex[i];
        bool meshHasTexture = (materialIndex < m_textureList.size() && m_textureList[materialIndex] != nullptr);
        
        if (useTextureUniformLoc != -1) {
            glUniform1i(useTextureUniformLoc, meshHasTexture ? 1 : 0);
        }
        
        if (meshHasTexture) {
            m_textureList[materialIndex]->UseTexture();
        }
        
        m_meshList[i]->RenderMesh();
    }
}

void Model::ClearModel() {
    // Clean up meshes
    for (size_t i = 0; i < m_meshList.size(); i++) {
        if (m_meshList[i]) {
            delete m_meshList[i];
            m_meshList[i] = nullptr;
        }
    }
    m_meshList.clear();
    
    // Clean up textures
    for (size_t i = 0; i < m_textureList.size(); i++) {
        if (m_textureList[i]) {
            delete m_textureList[i];
            m_textureList[i] = nullptr;
        }
    }
    m_textureList.clear();
    
    m_meshToTex.clear();
    
    // Clear bone names
    m_boneNames.clear();
    
    // Clear vertex positions
    m_vertexPositions.clear();
    
    // Clear heightmap data
    m_heightGrid.clear();
    m_heightGridW = m_heightGridH = 0;
    m_terrainSizeX = m_terrainSizeZ = m_terrainHeightScale = 0.0f;
    m_collisionTriangles.clear();
}

void Model::CreatePlane(float sizeX, float sizeZ) {
    // Clear any existing meshes
    ClearModel();
    
    // Create a simple plane mesh
    // Vertex format: 8 floats per vertex (3 position, 2 UV, 3 normal)
    // Plane is centered at origin, facing up (normal = 0, 1, 0)
    float halfX = sizeX * 0.5f;
    float halfZ = sizeZ * 0.5f;
    
    // 4 vertices for the plane quad
    GLfloat vertices[] = {
        // Position (X, Y, Z)        // Texture Coords (U, V)    // Normal (X, Y, Z)
        -halfX, 0.0f, -halfZ,        0.0f, 0.0f,                 0.0f, 1.0f, 0.0f,  // Top-left
         halfX, 0.0f, -halfZ,        1.0f, 0.0f,                 0.0f, 1.0f, 0.0f,  // Top-right
         halfX, 0.0f,  halfZ,        1.0f, 1.0f,                 0.0f, 1.0f, 0.0f,  // Bottom-right
        -halfX, 0.0f,  halfZ,        0.0f, 1.0f,                 0.0f, 1.0f, 0.0f   // Bottom-left
    };
    
    // 2 triangles (6 indices)
    unsigned int indices[] = {
        0, 1, 2,  // First triangle
        0, 2, 3   // Second triangle
    };
    
    // Create the mesh (4 vertices, 6 indices)
    Mesh* planeMesh = new Mesh();
    planeMesh->CreateMesh(vertices, indices, 4, 6);
    AppendCollisionTriangles(vertices, indices, 4, 6);
    
    m_meshList.push_back(planeMesh);
    m_meshToTex.push_back(0); // Use texture index 0
    
    // No texture for plane - RenderSystem will bind default white texture
    m_textureList.resize(1);
    m_textureList[0] = nullptr;
    
    // Set bounds for plane (centered at origin, flat on XZ plane)
    m_minX = -halfX;
    m_maxX = halfX;
    m_minY = 0.0f;
    m_maxY = 0.0f;
    m_minZ = -halfZ;
    m_maxZ = halfZ;
    m_boundsCalculated = true;
}

bool Model::CreateFromHeightmap(const std::string& imagePath, float sizeX, float sizeZ, float heightScale) {
    ClearModel();
    
    // Load with RGB (3 channels) for height + color; force 3 for consistent layout
    int width = 0, height = 0, channels = 0;
    unsigned char* data = stbi_load(imagePath.c_str(), &width, &height, &channels, 3);
    if (!data) {
        LOG_ERROR_FMT("Model", "Failed to load heightmap: %s", imagePath.c_str());
        return false;
    }
    
    const int bytesPerPixel = 3; // R, G, B
    const int gridW = width;
    const int gridH = height;
    const int numVertices = gridW * gridH;
    const int numIndices = (gridW - 1) * (gridH - 1) * 6;
    
    std::vector<GLfloat> vertices;
    std::vector<unsigned int> indices;
    std::vector<GLfloat> vertexColors;
    vertices.reserve(numVertices * 8);
    indices.reserve(numIndices);
    vertexColors.reserve(numVertices * 3);
    
    // Build height grid from luminance, extract vertex colors from RGB
    m_heightGrid.resize(gridW * gridH, 0.0f);
    float lumMin = 1.0f, lumMax = 0.0f;
    for (int row = 0; row < gridH; ++row) {
        for (int col = 0; col < gridW; ++col) {
            int idx = (row * gridW + col) * bytesPerPixel;
            float r = static_cast<float>(data[idx + 0]) / 255.0f;
            float g = static_cast<float>(data[idx + 1]) / 255.0f;
            float b = static_cast<float>(data[idx + 2]) / 255.0f;
            float lum = 0.299f * r + 0.587f * g + 0.114f * b;
            lumMin = std::min(lumMin, lum);
            lumMax = std::max(lumMax, lum);
            m_heightGrid[row * gridW + col] = lum;
            vertexColors.push_back(r);
            vertexColors.push_back(g);
            vertexColors.push_back(b);
        }
    }
    stbi_image_free(data);
    
    // Normalize height range: map [lumMin,lumMax] -> [0,1]
    float lumRange = lumMax - lumMin;
    if (lumRange > 0.0001f) {
        for (float& h : m_heightGrid) {
            h = (h - lumMin) / lumRange;
        }
    }
    
    LOG_INFO_FMT("Model", "Heightmap %dx%d: luminance %.3f-%.3f, %d vertices (1:1 pixel, RGB colors)",
        gridW, gridH, lumMin, lumMax, numVertices);
    
    m_heightGridW = gridW;
    m_heightGridH = gridH;
    m_terrainSizeX = sizeX;
    m_terrainSizeZ = sizeZ;
    m_terrainHeightScale = heightScale;
    
    // World position: pixel (row,col) -> world (row*sizeX/(H-1), height, col*sizeZ/(W-1))
    // Span 0 to sizeX, 0 to sizeZ
    float stepX = (gridH > 1) ? sizeX / static_cast<float>(gridH - 1) : 0.0f;
    float stepZ = (gridW > 1) ? sizeZ / static_cast<float>(gridW - 1) : 0.0f;
    
    for (int row = 0; row < gridH; ++row) {
        for (int col = 0; col < gridW; ++col) {
            float worldX = static_cast<float>(row) * stepX;
            float worldZ = static_cast<float>(col) * stepZ;
            float worldY = m_heightGrid[row * gridW + col] * heightScale;
            float u = (gridW > 1) ? static_cast<float>(col) / static_cast<float>(gridW - 1) : 0.0f;
            float v = (gridH > 1) ? static_cast<float>(row) / static_cast<float>(gridH - 1) : 0.0f;
            
            vertices.push_back(worldX);
            vertices.push_back(worldY);
            vertices.push_back(worldZ);
            vertices.push_back(u);
            vertices.push_back(v);
            vertices.push_back(0.0f);
            vertices.push_back(1.0f);
            vertices.push_back(0.0f);
        }
    }
    
    // Compute normals from height differences
    auto getHeight = [&](int r, int c) -> float {
        if (r < 0 || r >= gridH || c < 0 || c >= gridW) return 0.0f;
        return m_heightGrid[r * gridW + c] * heightScale;
    };
    
    for (int row = 0; row < gridH; ++row) {
        for (int col = 0; col < gridW; ++col) {
            float left = getHeight(row, col - 1);
            float right = getHeight(row, col + 1);
            float down = getHeight(row - 1, col);
            float up = getHeight(row + 1, col);
            
            glm::vec3 normal(
                (down - up) / (2.0f * (stepX > 0.0f ? stepX : 1.0f)),
                2.0f,
                (left - right) / (2.0f * (stepZ > 0.0f ? stepZ : 1.0f))
            );
            normal = glm::normalize(normal);
            
            int idx = (row * gridW + col) * 8;
            vertices[idx + 5] = normal.x;
            vertices[idx + 6] = normal.y;
            vertices[idx + 7] = normal.z;
        }
    }
    
    // Quads: each quad (row,col) connects pixels (row,col), (row,col+1), (row+1,col+1), (row+1,col)
    for (int row = 0; row < gridH - 1; ++row) {
        for (int col = 0; col < gridW - 1; ++col) {
            unsigned int tl = row * gridW + col;
            unsigned int tr = tl + 1;
            unsigned int bl = (row + 1) * gridW + col;
            unsigned int br = bl + 1;
            
            indices.push_back(tl);
            indices.push_back(tr);
            indices.push_back(bl);
            
            indices.push_back(tr);
            indices.push_back(br);
            indices.push_back(bl);
        }
    }
    
    Mesh* terrainMesh = new Mesh();
    terrainMesh->CreateMesh(vertices.data(), indices.data(),
        static_cast<unsigned int>(vertices.size() / 8),
        static_cast<unsigned int>(indices.size()));
    // Heightmap uses analytic RaycastHeightmap — do not build multi-million-triangle soup.
    terrainMesh->SetVertexColors(vertexColors.data(), static_cast<unsigned int>(vertexColors.size() / 3));
    
    m_meshList.push_back(terrainMesh);
    m_meshToTex.push_back(0);
    m_textureList.resize(1);
    m_textureList[0] = nullptr;
    
    // Bounds (terrain spans 0 to sizeX, 0 to sizeZ)
    m_minX = 0.0f;
    m_maxX = sizeX;
    m_minZ = 0.0f;
    m_maxZ = sizeZ;
    m_minY = 0.0f;
    m_maxY = heightScale;
    for (float h : m_heightGrid) {
        float y = h * heightScale;
        m_minY = std::min(m_minY, y);
        m_maxY = std::max(m_maxY, y);
    }
    m_boundsCalculated = true;
    
    LOG_INFO_FMT("Model", "Heightmap terrain loaded: %s (%dx%d), %.1fx%.1f units, origin at (0,0)",
        imagePath.c_str(), gridW, gridH, sizeX, sizeZ);
    return true;
}

bool Model::HasHeightmapTerrain() const {
    return !m_heightGrid.empty() && m_heightGridW > 0 && m_heightGridH > 0 &&
           m_terrainSizeX > 0.0f && m_terrainSizeZ > 0.0f;
}

float Model::GetHeightAt(float worldX, float worldZ) const {
    if (m_heightGrid.empty() || m_heightGridW <= 0 || m_heightGridH <= 0) {
        return 0.0f;
    }
    
    // Map world to pixel grid (1:1 mapping)
    float stepX = (m_heightGridH > 1) ? m_terrainSizeX / static_cast<float>(m_heightGridH - 1) : 0.0f;
    float stepZ = (m_heightGridW > 1) ? m_terrainSizeZ / static_cast<float>(m_heightGridW - 1) : 0.0f;
    if (stepX <= 0.0f || stepZ <= 0.0f) return m_heightGrid[0] * m_terrainHeightScale;
    
    float fx = worldX / stepX;
    float fz = worldZ / stepZ;
    
    if (fx < 0.0f || fx > static_cast<float>(m_heightGridH - 1) ||
        fz < 0.0f || fz > static_cast<float>(m_heightGridW - 1)) {
        return 0.0f;
    }
    
    int row0 = static_cast<int>(std::floor(fx));
    int col0 = static_cast<int>(std::floor(fz));
    row0 = std::max(0, std::min(row0, m_heightGridH - 1));
    col0 = std::max(0, std::min(col0, m_heightGridW - 1));
    int row1 = std::min(row0 + 1, m_heightGridH - 1);
    int col1 = std::min(col0 + 1, m_heightGridW - 1);
    
    float tx = fx - static_cast<float>(row0);
    float tz = fz - static_cast<float>(col0);
    
    float h00 = m_heightGrid[row0 * m_heightGridW + col0] * m_terrainHeightScale;
    float h01 = m_heightGrid[row0 * m_heightGridW + col1] * m_terrainHeightScale;
    float h10 = m_heightGrid[row1 * m_heightGridW + col0] * m_terrainHeightScale;
    float h11 = m_heightGrid[row1 * m_heightGridW + col1] * m_terrainHeightScale;
    
    float h0 = h00 * (1.0f - tz) + h01 * tz;
    float h1 = h10 * (1.0f - tz) + h11 * tz;
    return h0 * (1.0f - tx) + h1 * tx;
}

void Model::CreateSphere(float radius, unsigned int segments) {
    ClearModel();
    
    if (segments < 3) segments = 3;
    
    std::vector<GLfloat> vertices;
    std::vector<unsigned int> indices;
    
    const float PI = 3.14159265358979323846f;
    
    // Generate sphere vertices using latitude/longitude method
    for (unsigned int y = 0; y <= segments; ++y) {
        float theta = static_cast<float>(y) * PI / static_cast<float>(segments);
        float sinTheta = sinf(theta);
        float cosTheta = cosf(theta);
        
        for (unsigned int x = 0; x <= segments; ++x) {
            float phi = static_cast<float>(x) * 2.0f * PI / static_cast<float>(segments);
            float sinPhi = sinf(phi);
            float cosPhi = cosf(phi);
            
            // Position
            float px = cosPhi * sinTheta;
            float py = cosTheta;
            float pz = sinPhi * sinTheta;
            
            // Normal (same as position for unit sphere, pointing outward)
            float nx = px;
            float ny = py;
            float nz = pz;
            
            // UV coordinates
            float u = static_cast<float>(x) / static_cast<float>(segments);
            float v = static_cast<float>(y) / static_cast<float>(segments);
            
            // Add vertex data: position (3), UV (2), normal (3) = 8 floats
            vertices.push_back(px * radius); // X
            vertices.push_back(py * radius); // Y
            vertices.push_back(pz * radius); // Z
            vertices.push_back(u);            // U
            vertices.push_back(v);            // V
            vertices.push_back(nx);           // NX
            vertices.push_back(ny);           // NY
            vertices.push_back(nz);           // NZ
        }
    }
    
    // Generate indices with correct winding order (counter-clockwise for outward-facing)
    for (unsigned int y = 0; y < segments; ++y) {
        for (unsigned int x = 0; x < segments; ++x) {
            unsigned int first = y * (segments + 1) + x;
            unsigned int second = first + segments + 1;
            
            // First triangle: (x,y) -> (x+1,y) -> (x,y+1) - CCW winding
            indices.push_back(first);
            indices.push_back(first + 1);
            indices.push_back(second);
            
            // Second triangle: (x+1,y) -> (x+1,y+1) -> (x,y+1) - CCW winding
            indices.push_back(first + 1);
            indices.push_back(second + 1);
            indices.push_back(second);
        }
    }
    
    // Create the mesh (vertices.size() = 8 floats per vertex)
    Mesh* sphereMesh = new Mesh();
    sphereMesh->CreateMesh(vertices.data(), indices.data(),
        static_cast<unsigned int>(vertices.size() / 8),
        static_cast<unsigned int>(indices.size()));
    AppendCollisionTriangles(vertices.data(), indices.data(),
                             static_cast<unsigned int>(vertices.size() / 8),
                             static_cast<unsigned int>(indices.size()));
    
    m_meshList.push_back(sphereMesh);
    m_meshToTex.push_back(0);
    
    // No texture initially - will be set externally
    m_textureList.resize(1);
    m_textureList[0] = nullptr;
    
    // Set bounds
    m_minX = m_minY = m_minZ = -radius;
    m_maxX = m_maxY = m_maxZ = radius;
    m_boundsCalculated = true;
}

void Model::CalculateBounds(const aiScene* scene) {
    // Reset bounds
    m_minX = m_minY = m_minZ = std::numeric_limits<float>::max();
    m_maxX = m_maxY = m_maxZ = std::numeric_limits<float>::lowest();
    
    // Iterate through all meshes to find the bounding box
    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        const aiMesh* mesh = scene->mMeshes[i];
        
        // Check all vertices in this mesh
        for (unsigned int j = 0; j < mesh->mNumVertices; j++) {
            const aiVector3D& vertex = mesh->mVertices[j];
            
            // Update min/max for each axis
            if (vertex.x < m_minX) m_minX = vertex.x;
            if (vertex.x > m_maxX) m_maxX = vertex.x;
            
            if (vertex.y < m_minY) m_minY = vertex.y;
            if (vertex.y > m_maxY) m_maxY = vertex.y;
            
            if (vertex.z < m_minZ) m_minZ = vertex.z;
            if (vertex.z > m_maxZ) m_maxZ = vertex.z;
        }
    }
    
    m_boundsCalculated = true;
}

void Model::GetBounds(float& minX, float& maxX, float& minY, float& maxY, float& minZ, float& maxZ) const {
    minX = m_minX;
    maxX = m_maxX;
    minY = m_minY;
    maxY = m_maxY;
    minZ = m_minZ;
    maxZ = m_maxZ;
}

namespace {

float MeshMaxExtentAndAxes(const std::vector<glm::vec3>& pos,
                           float& outSx, float& outSy, float& outSz) {
    if (pos.empty()) {
        outSx = outSy = outSz = 0.0f;
        return 0.0f;
    }
    glm::vec3 mn = pos[0];
    glm::vec3 mx = pos[0];
    for (const glm::vec3& p : pos) {
        mn = glm::min(mn, p);
        mx = glm::max(mx, p);
    }
    outSx = mx.x - mn.x;
    outSy = mx.y - mn.y;
    outSz = mx.z - mn.z;
    return std::max(outSx, std::max(outSy, outSz));
}

bool MeshIsLikelyThinPlate(float sx, float sy, float sz) {
    const float maxD = std::max(sx, std::max(sy, sz));
    const float minD = std::min(sx, std::min(sy, sz));
    if (maxD < 1e-6f) {
        return true;
    }
    return minD < 0.05f * maxD;
}

} // namespace

float Model::GetUniformScaleReferenceExtent() const {
    const float combined =
        std::max(m_maxX - m_minX, std::max(m_maxY - m_minY, m_maxZ - m_minZ));

    if (m_vertexPositions.empty()) {
        return combined;
    }

    // Per-mesh max extents. Skip thin plates (large ground planes) first; if that removes
    // everything, include all meshes so single-plane assets still work.
    auto collectExtents = [this](bool skipThinPlates) -> std::vector<float> {
        std::vector<float> extents;
        for (const auto& pos : m_vertexPositions) {
            if (pos.empty()) {
                continue;
            }
            float sx = 0.0f;
            float sy = 0.0f;
            float sz = 0.0f;
            const float ext = MeshMaxExtentAndAxes(pos, sx, sy, sz);
            if (skipThinPlates && MeshIsLikelyThinPlate(sx, sy, sz)) {
                continue;
            }
            extents.push_back(ext);
        }
        return extents;
    };

    std::vector<float> extents = collectExtents(true);
    if (extents.empty()) {
        extents = collectExtents(false);
    }
    if (extents.empty()) {
        return combined;
    }

    // Drop obvious outliers (e.g. one huge mesh vs. a character): if the largest extent is
    // many times the next, remove it until stable. Matches world units to the main subject.
    std::sort(extents.begin(), extents.end(), std::greater<float>());
    constexpr float kOutlierRatio = 5.0f;
    while (extents.size() >= 2 && extents[0] >= kOutlierRatio * extents[1]) {
        extents.erase(extents.begin());
    }

    return extents[0];
}

std::string Model::GetBoneNameByIndex(unsigned int meshBoneIndex) const {
    if (meshBoneIndex < m_boneNames.size()) {
        return m_boneNames[meshBoneIndex];
    }
    return std::string(); // Return empty string if index is invalid
}

void Model::UpdateVertexPositions(std::vector<std::vector<GLfloat>>& vertexData,
                                   const std::vector<unsigned int>& meshVertexCounts) {
    if (vertexData.size() != m_meshList.size() || meshVertexCounts.size() != m_meshList.size()) {
        LOG_WARNING_FMT("Model", "Vertex data count mismatch: %zu meshes, %zu vertex data arrays", 
                        m_meshList.size(), vertexData.size());
        return;
    }
    
    for (size_t i = 0; i < m_meshList.size() && i < vertexData.size(); i++) {
        if (!vertexData[i].empty() && meshVertexCounts[i] > 0) {
            m_meshList[i]->UpdateVertexPositions(vertexData[i].data(), meshVertexCounts[i]);
        }
    }
}

void Model::ApplyRainbowColors(float minZ, float maxZ) {
    if (m_meshList.empty() || m_vertexPositions.size() != m_meshList.size()) {
        return;
    }
    
    float zRange = maxZ - minZ;
    if (zRange < 0.0001f) {
        zRange = 1.0f;
    }
    
    for (size_t i = 0; i < m_meshList.size() && i < m_vertexPositions.size(); i++) {
        if (!m_meshList[i]) continue;
        
        const std::vector<glm::vec3>& vertexPositions = m_vertexPositions[i];
        if (vertexPositions.empty()) continue;
        
        // Generate colors for each vertex based on its Z position
        std::vector<GLfloat> colors;
        colors.reserve(vertexPositions.size() * 3);
        
        for (const glm::vec3& pos : vertexPositions) {
            float normalizedZ = (pos.z - minZ) / zRange;
            normalizedZ = std::max(0.0f, std::min(1.0f, normalizedZ));
            // Invert normalizedZ so colors match the point cloud mesh orientation
            normalizedZ = 1.0f - normalizedZ;
            
            // Generate rainbow color based on normalized Z (0 = red, 1 = violet)
            glm::vec3 color;
            float hue = normalizedZ * 5.0f; // 0-5 maps to full rainbow
            
            if (hue < 1.0f) {
                // Red to Yellow
                color = glm::vec3(1.0f, hue, 0.0f);
            } else if (hue < 2.0f) {
                // Yellow to Green
                color = glm::vec3(2.0f - hue, 1.0f, 0.0f);
            } else if (hue < 3.0f) {
                // Green to Cyan
                color = glm::vec3(0.0f, 1.0f, hue - 2.0f);
            } else if (hue < 4.0f) {
                // Cyan to Blue
                color = glm::vec3(0.0f, 4.0f - hue, 1.0f);
            } else {
                // Blue to Magenta
                color = glm::vec3(hue - 4.0f, 0.0f, 1.0f);
            }
            
            colors.push_back(color.r);
            colors.push_back(color.g);
            colors.push_back(color.b);
        }
        
        // Apply colors to mesh
        if (!colors.empty()) {
            GLfloat* colorData = colors.data();
            unsigned int vertexCount = static_cast<unsigned int>(vertexPositions.size());
            m_meshList[i]->SetVertexColors(colorData, vertexCount);
        }
    }
}

Model::~Model() {
    ClearModel();
}

void Model::AppendCollisionTriangles(const GLfloat* vertices, const unsigned int* indices,
                                     unsigned int numVertices, unsigned int numIndices) {
    if (!vertices || !indices || numVertices == 0 || numIndices < 3) {
        return;
    }

    // Guard against accidental huge soups (e.g. high-res heightmaps).
    constexpr size_t kMaxCollisionTriangles = 50000;
    const size_t incomingTriangles = numIndices / 3;
    if (m_collisionTriangles.size() / 3 + incomingTriangles > kMaxCollisionTriangles) {
        return;
    }

    constexpr unsigned int kFloatsPerVertex = 8;
    for (unsigned int tri = 0; tri + 2 < numIndices; tri += 3) {
        for (unsigned int corner = 0; corner < 3; ++corner) {
            const unsigned int vertexIndex = indices[tri + corner];
            if (vertexIndex >= numVertices) {
                continue;
            }
            const unsigned int base = vertexIndex * kFloatsPerVertex;
            m_collisionTriangles.emplace_back(vertices[base + 0], vertices[base + 1], vertices[base + 2]);
        }
    }
}

bool Model::RaycastLocal(const glm::vec3& origin, const glm::vec3& dir, float maxDistance,
                         float& outDistance) const {
    if (m_collisionTriangles.empty() || maxDistance <= 0.0f) {
        return false;
    }

    bool hit = false;
    float closest = maxDistance;
    for (size_t tri = 0; tri + 2 < m_collisionTriangles.size(); tri += 3) {
        float t = 0.0f;
        if (Raycast::IntersectTriangle(origin, dir, m_collisionTriangles[tri], m_collisionTriangles[tri + 1],
                                       m_collisionTriangles[tri + 2], t)) {
            if (t >= 0.0f && t < closest) {
                closest = t;
                hit = true;
            }
        }
    }

    if (!hit) {
        return false;
    }

    outDistance = closest;
    return true;
}

bool Model::RaycastHeightmap(const glm::vec3& localOrigin, const glm::vec3& localDir, float maxDistance,
                             float& outDistance) const {
    if (!HasHeightmapTerrain() || maxDistance <= 0.0f) {
        return false;
    }

    const float dirLen = glm::length(localDir);
    if (dirLen < 1e-6f) {
        return false;
    }
    const glm::vec3 dir = localDir / dirLen;

    const float stepX =
        (m_heightGridH > 1) ? m_terrainSizeX / static_cast<float>(m_heightGridH - 1) : 1.0f;
    const float stepZ =
        (m_heightGridW > 1) ? m_terrainSizeZ / static_cast<float>(m_heightGridW - 1) : 1.0f;
    const float step = std::max(0.2f, std::min(stepX, stepZ) * 0.5f);

    constexpr float kSurfaceSkin = 0.15f;
    for (float t = 0.0f; t <= maxDistance; t += step) {
        const glm::vec3 point = localOrigin + dir * t;
        const float terrainHeight = GetHeightAt(point.x, point.z);
        if (point.y <= terrainHeight + kSurfaceSkin) {
            outDistance = t;
            return true;
        }
    }

    return false;
}

bool Model::RaycastWorld(const glm::mat4& modelMatrix, const glm::vec3& origin, const glm::vec3& dir,
                         float maxDistance, float& outDistance) const {
    if (maxDistance <= 0.0f || m_collisionTriangles.empty()) {
        return false;
    }

    const glm::mat4 inverseModel = glm::inverse(modelMatrix);
    const glm::vec3 localOrigin = glm::vec3(inverseModel * glm::vec4(origin, 1.0f));
    const glm::vec3 worldEnd = origin + dir * maxDistance;
    const glm::vec3 localEnd = glm::vec3(inverseModel * glm::vec4(worldEnd, 1.0f));
    const glm::vec3 localSegment = localEnd - localOrigin;
    const float localMaxDistance = glm::length(localSegment);
    if (localMaxDistance <= 1e-5f) {
        return false;
    }

    const glm::vec3 localDir = localSegment / localMaxDistance;

    if (HasHeightmapTerrain()) {
        float localHitDistance = 0.0f;
        if (!RaycastHeightmap(localOrigin, localDir, localMaxDistance, localHitDistance)) {
            return false;
        }

        const glm::vec3 worldHit =
            glm::vec3(modelMatrix * glm::vec4(localOrigin + localDir * localHitDistance, 1.0f));
        const float worldDistance = glm::dot(worldHit - origin, dir);
        if (worldDistance < 0.0f || worldDistance > maxDistance) {
            return false;
        }

        outDistance = worldDistance;
        return true;
    }

    float localHitDistance = 0.0f;
    if (!RaycastLocal(localOrigin, localDir, localMaxDistance, localHitDistance)) {
        return false;
    }

    const glm::vec3 worldHit = glm::vec3(modelMatrix * glm::vec4(localOrigin + localDir * localHitDistance, 1.0f));
    const float worldDistance = glm::dot(worldHit - origin, dir);
    if (worldDistance < 0.0f || worldDistance > maxDistance) {
        return false;
    }

    outDistance = worldDistance;
    return true;
}

} // namespace Render
} // namespace Platform
} // namespace Engine

