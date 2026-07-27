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
    // UI sprites: clamp edges, mipmaps, optional downscale for sharp HUD rendering.
    bool LoadTextureUI(int maxSourceDimension = 1024);
    
    void UseTexture();
    void ClearTexture();

    GLuint GetID() const { return textureID; }
    int GetWidth() const { return width; }
    int GetHeight() const { return height; }
    bool IsLoaded() const { return textureID != 0; }
    
    ~Texture();

private:
    GLuint textureID;
    int width, height, bitDepth;
    const char* fileLocation;
};

} // namespace Render
} // namespace Platform
} // namespace Engine



