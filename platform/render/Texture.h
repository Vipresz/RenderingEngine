#pragma once

#include <GL/glew.h>

namespace Engine {
namespace Platform {
namespace Render {

class Texture {
public:
    Texture();
    Texture(const char* fileLoc);
    
    bool LoadTexture();
    bool LoadTextureA();
    /// Decode image bytes (e.g. Assimp embedded JPEG/PNG). Returns false if stb cannot decode.
    bool LoadFromMemory(const unsigned char* buffer, int len);
    
    void UseTexture();
    void ClearTexture();
    
    ~Texture();

private:
    GLuint textureID;
    int width, height, bitDepth;
    const char* fileLocation;
};

} // namespace Render
} // namespace Platform
} // namespace Engine



