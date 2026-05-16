#include "render/Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <cstdio>

namespace Engine {
namespace Platform {
namespace Render {

Texture::Texture() {
    textureID = 0;
    width = 0;
    height = 0;
    bitDepth = 0;
    fileLocation = "";
}

Texture::Texture(const char* fileLoc) {
    textureID = 0;
    width = 0;
    height = 0;
    bitDepth = 0;
    fileLocation = fileLoc;
}

bool Texture::LoadTexture() {
    int channelsInFile = 0;
    unsigned char* texData =
        stbi_load(fileLocation, &width, &height, &channelsInFile, STBI_rgb);
    if (!texData) {
        printf("Failed to find or decode RGB texture: %s\n", fileLocation ? fileLocation : "");
        return false;
    }
    bitDepth = 3;

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, texData);
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(texData);

    return true;
}

bool Texture::LoadTextureA() {
    int channelsInFile = 0;
    unsigned char* texData =
        stbi_load(fileLocation, &width, &height, &channelsInFile, STBI_rgb_alpha);
    if (!texData) {
        printf("Failed to find or decode RGBA texture: %s\n", fileLocation ? fileLocation : "");
        return false;
    }
    bitDepth = 4;

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(texData);

    return true;
}

bool Texture::LoadFromMemory(const unsigned char* buffer, int len) {
    if (!buffer || len <= 0) {
        return false;
    }

    int channelsInFile = 0;
    unsigned char* texData =
        stbi_load_from_memory(buffer, len, &width, &height, &channelsInFile, STBI_rgb_alpha);
    if (!texData) {
        channelsInFile = 0;
        texData = stbi_load_from_memory(buffer, len, &width, &height, &channelsInFile, STBI_rgb);
    }
    if (!texData) {
        return false;
    }
    bitDepth = (channelsInFile == 4) ? 4 : 3;

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    if (bitDepth == 4) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, texData);
    }
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(texData);
    return true;
}

void Texture::UseTexture() {
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textureID);
}

void Texture::ClearTexture() {
    if (textureID != 0) {
        glDeleteTextures(1, &textureID);
        textureID = 0;
        width = 0;
        height = 0;
        bitDepth = 0;
    }
}

Texture::~Texture() {
    ClearTexture();
}

} // namespace Render
} // namespace Platform
} // namespace Engine



