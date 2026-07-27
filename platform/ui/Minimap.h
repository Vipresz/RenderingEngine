#pragma once

#ifdef PLATFORM_HAS_NUKLEAR

#include "core/Types.h"
#include "core/ecs/Entity.h"
#include "render/Texture.h"

#include <cstddef>
#include <memory>

struct nk_context;

namespace Engine {
namespace ECS {
class Registry;
}

namespace Platform {
namespace Render {
class Model;
}

namespace UI {

// Top-right HUD minimap: terrain image with a marker for the tracked entity.
class Minimap {
public:
    bool Initialize(const char* mapTexturePath,
                    const Render::Model* terrain,
                    float fallbackWorldSizeX = 100.0f,
                    float fallbackWorldSizeZ = 100.0f,
                    const char* borderTexturePath = nullptr);
    ~Minimap();
    void Shutdown();

    void SetTerrain(const Render::Model* terrain);
    void SetTrackedEntity(EntityID entity) { m_trackedEntity = entity; }
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }

    void SetSizePixels(float size) { m_designSizePixels = size; }
    void SetMarginPixels(float margin) { m_designMarginPixels = margin; }
    void SetBorderInsetFraction(float fraction) { m_borderInsetFraction = fraction; }
    void SetReferenceScreen(int width, int height);

    void Draw(struct nk_context* ctx,
              ECS::Registry& registry,
              int screenWidth,
              int screenHeight,
              const EntityID* otherPlayerEntities = nullptr,
              size_t otherPlayerCount = 0) const;

private:
    bool ResolveWorldBounds(const Render::Model* terrain,
                            float& outOriginX,
                            float& outOriginZ,
                            float& outSizeX,
                            float& outSizeZ) const;

    std::unique_ptr<Render::Texture> m_mapTexture;
    std::unique_ptr<Render::Texture> m_borderTexture;
    const Render::Model* m_terrain = nullptr;
    EntityID m_trackedEntity = INVALID_ENTITY;
    float m_fallbackWorldSizeX = 100.0f;
    float m_fallbackWorldSizeZ = 100.0f;
    float m_designSizePixels = 234.0f;
    float m_designMarginPixels = 21.0f;
    int m_referenceWidth = 1280;
    int m_referenceHeight = 720;
    float m_borderInsetFraction = 0.13f;
    bool m_enabled = true;
};

} // namespace UI
} // namespace Platform
} // namespace Engine

#endif // PLATFORM_HAS_NUKLEAR
