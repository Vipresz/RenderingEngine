#include "ui/Minimap.h"

#include "core/ecs/Registry.h"
#include "render/Model.h"
#include "render/components/TransformComponent.h"
#include "ui/OverlayWindow.h"

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#include "nuklear.h"

#include <algorithm>

using Engine::Platform::Render::Components::TransformComponent;

namespace Engine {
namespace Platform {
namespace UI {

namespace {

float ComputeUiScale(int screenWidth, int screenHeight, int referenceWidth, int referenceHeight) {
    if (screenWidth <= 0 || screenHeight <= 0 || referenceWidth <= 0 || referenceHeight <= 0) {
        return 1.0f;
    }

    const float scaleX = static_cast<float>(screenWidth) / static_cast<float>(referenceWidth);
    const float scaleY = static_cast<float>(screenHeight) / static_cast<float>(referenceHeight);
    return std::min(scaleX, scaleY);
}

float WorldToNormalized(float value, float origin, float size) {
    if (size <= 0.0f) {
        return 0.0f;
    }
    return (value - origin) / size;
}

float ClampMapPan(float desiredPan, float normalizedPosition) {
    const float minPan = normalizedPosition - 0.5f;
    const float maxPan = 0.5f - normalizedPosition;
    if (minPan <= maxPan) {
        return std::clamp(desiredPan, minPan, maxPan);
    }
    return normalizedPosition < 0.5f ? maxPan : minPan;
}

void DrawPlayerMarker(struct nk_command_buffer* canvas,
                      float centerX,
                      float centerY,
                      float radius,
                      float outlineRadius,
                      struct nk_color fillColor) {
    if (!canvas || radius <= 0.0f) {
        return;
    }

    DrawFilledCircle(canvas, centerX, centerY, outlineRadius, nk_rgba(0, 0, 0, 220));
    DrawFilledCircle(canvas, centerX, centerY, radius, fillColor);
}

} // namespace

bool Minimap::Initialize(const char* mapTexturePath,
                         const Render::Model* terrain,
                         float fallbackWorldSizeX,
                         float fallbackWorldSizeZ,
                         const char* borderTexturePath) {
    Shutdown();

    if (!mapTexturePath || mapTexturePath[0] == '\0') {
        return false;
    }

    m_fallbackWorldSizeX = fallbackWorldSizeX;
    m_fallbackWorldSizeZ = fallbackWorldSizeZ;
    m_terrain = terrain;

    m_mapTexture = std::make_unique<Render::Texture>(mapTexturePath);
    if (!m_mapTexture->LoadTextureUI(512)) {
        m_mapTexture.reset();
        return false;
    }

    if (borderTexturePath && borderTexturePath[0] != '\0') {
        m_borderTexture = std::make_unique<Render::Texture>(borderTexturePath);
        if (!m_borderTexture->LoadTextureUI(512)) {
            m_borderTexture.reset();
        }
    }

    return true;
}

Minimap::~Minimap() {
    Shutdown();
}

void Minimap::Shutdown() {
    if (m_mapTexture) {
        m_mapTexture->ClearTexture();
        m_mapTexture.reset();
    }
    if (m_borderTexture) {
        m_borderTexture->ClearTexture();
        m_borderTexture.reset();
    }
}

void Minimap::SetTerrain(const Render::Model* terrain) {
    m_terrain = terrain;
}

void Minimap::SetReferenceScreen(int width, int height) {
    if (width > 0) {
        m_referenceWidth = width;
    }
    if (height > 0) {
        m_referenceHeight = height;
    }
}

bool Minimap::ResolveWorldBounds(const Render::Model* terrain,
                                 float& outOriginX,
                                 float& outOriginZ,
                                 float& outSizeX,
                                 float& outSizeZ) const {
    outOriginX = 0.0f;
    outOriginZ = 0.0f;
    outSizeX = m_fallbackWorldSizeX;
    outSizeZ = m_fallbackWorldSizeZ;

    if (!terrain) {
        return false;
    }

    if (terrain->HasHeightmapTerrain()) {
        outSizeX = terrain->GetTerrainWorldSizeX();
        outSizeZ = terrain->GetTerrainWorldSizeZ();
        return true;
    }

    float minX = 0.0f;
    float maxX = 0.0f;
    float minY = 0.0f;
    float maxY = 0.0f;
    float minZ = 0.0f;
    float maxZ = 0.0f;
    terrain->GetBounds(minX, maxX, minY, maxY, minZ, maxZ);
    outOriginX = minX;
    outOriginZ = minZ;
    outSizeX = std::max(0.01f, maxX - minX);
    outSizeZ = std::max(0.01f, maxZ - minZ);
    return true;
}

void Minimap::Draw(struct nk_context* ctx,
                   ECS::Registry& registry,
                   int screenWidth,
                   int screenHeight,
                   const EntityID* otherPlayerEntities,
                   size_t otherPlayerCount) const {
    if (!ctx || !m_enabled || screenWidth <= 0 || screenHeight <= 0) {
        return;
    }

    if (!m_mapTexture || !m_mapTexture->IsLoaded()) {
        return;
    }

    float worldOriginX = 0.0f;
    float worldOriginZ = 0.0f;
    float worldSizeX = m_fallbackWorldSizeX;
    float worldSizeZ = m_fallbackWorldSizeZ;
    ResolveWorldBounds(m_terrain, worldOriginX, worldOriginZ, worldSizeX, worldSizeZ);

    const float uiScale =
        ComputeUiScale(screenWidth, screenHeight, m_referenceWidth, m_referenceHeight);
    const float size = m_designSizePixels * uiScale;
    const float margin = m_designMarginPixels * uiScale;
    const float x = static_cast<float>(screenWidth) - margin - size;
    const float y = margin;
    const struct nk_rect frameBounds = nk_rect(x, y, size, size);

    const bool hasBorder = m_borderTexture && m_borderTexture->IsLoaded();
    const float inset = hasBorder
        ? size * std::clamp(m_borderInsetFraction, 0.05f, 0.25f)
        : 3.0f * uiScale;
    const struct nk_rect innerBounds =
        nk_rect(frameBounds.x + inset, frameBounds.y + inset, size - inset * 2.0f, size - inset * 2.0f);
    const float mapDrawW = (frameBounds.w + innerBounds.w) * 0.5f;
    const float mapDrawH = (frameBounds.h + innerBounds.h) * 0.5f;
    const struct nk_rect mapBounds = nk_rect(
        frameBounds.x + (frameBounds.w - mapDrawW) * 0.5f,
        frameBounds.y + (frameBounds.h - mapDrawH) * 0.5f,
        mapDrawW,
        mapDrawH);

    float localNormalizedX = 0.5f;
    float localNormalizedZ = 0.5f;
    bool hasLocalPlayer = false;
    if (m_trackedEntity != INVALID_ENTITY &&
        registry.HasComponent<TransformComponent>(m_trackedEntity)) {
        const TransformComponent* localTransform = registry.GetComponent<TransformComponent>(m_trackedEntity);
        if (localTransform) {
            localNormalizedX =
                WorldToNormalized(localTransform->position.x, worldOriginX, worldSizeX);
            localNormalizedZ =
                WorldToNormalized(localTransform->position.z, worldOriginZ, worldSizeZ);
            hasLocalPlayer = true;
        }
    }

    const float panX =
        hasLocalPlayer ? ClampMapPan(0.5f - localNormalizedX, localNormalizedX) : 0.0f;
    const float panZ =
        hasLocalPlayer ? ClampMapPan(0.5f - localNormalizedZ, localNormalizedZ) : 0.0f;
    const struct nk_rect mapDest = nk_rect(
        mapBounds.x + panX * mapBounds.w,
        mapBounds.y + panZ * mapBounds.h,
        mapBounds.w,
        mapBounds.h);

    OverlayScope overlay(
        ctx,
        "Minimap",
        OverlayScope::FullScreen(screenWidth, screenHeight),
        NK_WINDOW_NO_INPUT);
    if (!overlay.IsOpen() || !overlay.Canvas()) {
        return;
    }

    struct nk_command_buffer* canvas = overlay.Canvas();
    if (!hasBorder) {
        const float cornerRadius = 6.0f * uiScale;
        const float strokeWidth = 2.0f * uiScale;
        nk_fill_rect(canvas, frameBounds, cornerRadius, nk_rgba(10, 12, 18, 220));
        nk_stroke_rect(canvas, frameBounds, cornerRadius, strokeWidth, nk_rgba(200, 180, 120, 255));
    }

    nk_push_scissor(canvas, mapBounds);
    DrawTexture(canvas, *m_mapTexture, mapDest, nk_rgba(255, 255, 255, 255));

    const float markerRadius = 4.5f * uiScale;
    const float markerOutlineRadius = markerRadius + 1.5f * uiScale;

    if (otherPlayerEntities) {
        for (size_t index = 0; index < otherPlayerCount; ++index) {
            const EntityID entityId = otherPlayerEntities[index];
            if (entityId == INVALID_ENTITY || entityId == m_trackedEntity ||
                !registry.HasComponent<TransformComponent>(entityId)) {
                continue;
            }

            const TransformComponent* transform = registry.GetComponent<TransformComponent>(entityId);
            if (!transform) {
                continue;
            }

            const float normalizedX =
                WorldToNormalized(transform->position.x, worldOriginX, worldSizeX);
            const float normalizedZ =
                WorldToNormalized(transform->position.z, worldOriginZ, worldSizeZ);
            const float markerX = mapBounds.x + (panX + normalizedX) * mapBounds.w;
            const float markerY = mapBounds.y + (panZ + normalizedZ) * mapBounds.h;

            DrawPlayerMarker(canvas,
                             markerX,
                             markerY,
                             markerRadius,
                             markerOutlineRadius,
                             nk_rgba(255, 255, 255, 255));
        }
    }

    if (hasLocalPlayer) {
        const float centerX = mapBounds.x + mapBounds.w * 0.5f;
        const float centerY = mapBounds.y + mapBounds.h * 0.5f;
        DrawPlayerMarker(canvas,
                         centerX,
                         centerY,
                         markerRadius + 0.5f * uiScale,
                         markerOutlineRadius + 0.5f * uiScale,
                         nk_rgba(255, 220, 80, 255));
    }

    nk_push_scissor(canvas,
                    nk_rect(0.0f,
                            0.0f,
                            static_cast<float>(screenWidth),
                            static_cast<float>(screenHeight)));

    if (hasBorder) {
        DrawTexture(canvas, *m_borderTexture, frameBounds, nk_rgba(255, 255, 255, 255));
    }
}

} // namespace UI
} // namespace Platform
} // namespace Engine
