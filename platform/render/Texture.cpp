#include "render/Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

// stb returns the top row first. OpenGL t=0 is the bottom, which is also OBJ v=0.
void FlipImageVertical(unsigned char* data, int width, int height, int channels) {
    if (data == nullptr || width <= 0 || height <= 1 || channels <= 0) {
        return;
    }
    const int stride = width * channels;
    std::vector<unsigned char> row(static_cast<size_t>(stride));
    for (int y = 0; y < height / 2; ++y) {
        unsigned char* top = data + static_cast<size_t>(y) * static_cast<size_t>(stride);
        unsigned char* bottom = data + static_cast<size_t>(height - 1 - y) * static_cast<size_t>(stride);
        std::memcpy(row.data(), top, static_cast<size_t>(stride));
        std::memcpy(top, bottom, static_cast<size_t>(stride));
        std::memcpy(bottom, row.data(), static_cast<size_t>(stride));
    }
}

}

namespace Engine {
namespace Platform {
namespace Render {

namespace {

void ResizeRgbaBox(const unsigned char* src,
                   int srcW,
                   int srcH,
                   unsigned char* dst,
                   int dstW,
                   int dstH) {
    for (int y = 0; y < dstH; ++y) {
        const int srcY0 = (y * srcH) / dstH;
        const int srcY1 = std::min(srcH, ((y + 1) * srcH) / dstH);
        for (int x = 0; x < dstW; ++x) {
            const int srcX0 = (x * srcW) / dstW;
            const int srcX1 = std::min(srcW, ((x + 1) * srcW) / dstW);

            int r = 0;
            int g = 0;
            int b = 0;
            int a = 0;
            int count = 0;
            for (int sy = srcY0; sy < srcY1; ++sy) {
                for (int sx = srcX0; sx < srcX1; ++sx) {
                    const unsigned char* pixel = src + (sy * srcW + sx) * 4;
                    r += pixel[0];
                    g += pixel[1];
                    b += pixel[2];
                    a += pixel[3];
                    ++count;
                }
            }

            unsigned char* out = dst + (y * dstW + x) * 4;
            if (count > 0) {
                out[0] = static_cast<unsigned char>(r / count);
                out[1] = static_cast<unsigned char>(g / count);
                out[2] = static_cast<unsigned char>(b / count);
                out[3] = static_cast<unsigned char>(a / count);
            }
        }
    }
}

std::vector<unsigned char> DownscaleRgbaToMaxDimension(const unsigned char* src,
                                                         int srcW,
                                                         int srcH,
                                                         int maxDimension,
                                                         int& outW,
                                                         int& outH) {
    outW = srcW;
    outH = srcH;
    if (maxDimension <= 0 || std::max(srcW, srcH) <= maxDimension) {
        return std::vector<unsigned char>(src, src + (srcW * srcH * 4));
    }

    const float scale =
        static_cast<float>(maxDimension) / static_cast<float>(std::max(srcW, srcH));
    outW = std::max(1, static_cast<int>(srcW * scale + 0.5f));
    outH = std::max(1, static_cast<int>(srcH * scale + 0.5f));

    std::vector<unsigned char> dst(static_cast<size_t>(outW) * static_cast<size_t>(outH) * 4);
    ResizeRgbaBox(src, srcW, srcH, dst.data(), outW, outH);
    return dst;
}

void UploadUiTexture(GLuint textureId, int uploadW, int uploadH, const unsigned char* uploadData) {
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 uploadW,
                 uploadH,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 uploadData);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace

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
    FlipImageVertical(texData, width, height, bitDepth);

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
    FlipImageVertical(texData, width, height, bitDepth);

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
    FlipImageVertical(texData, width, height, bitDepth);

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

bool Texture::LoadSolidColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    ClearTexture();
    const unsigned char pixel[4] = {r, g, b, a};
    width = 1;
    height = 1;
    bitDepth = 4;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    return textureID != 0;
}

bool Texture::LoadTextureUI(int maxSourceDimension) {
    int loadedW = 0;
    int loadedH = 0;
    unsigned char* texData = stbi_load(fileLocation, &loadedW, &loadedH, &bitDepth, 4);
    if (!texData) {
        printf("Failed to find: %s\n", fileLocation);
        return false;
    }

    int uploadW = loadedW;
    int uploadH = loadedH;
    std::vector<unsigned char> resized =
        DownscaleRgbaToMaxDimension(texData, loadedW, loadedH, maxSourceDimension, uploadW, uploadH);
    const unsigned char* uploadData = resized.data();

    width = uploadW;
    height = uploadH;

    glGenTextures(1, &textureID);
    UploadUiTexture(textureID, uploadW, uploadH, uploadData);

    stbi_image_free(texData);
    bitDepth = 4;

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
