#version 330 core

layout (location = 0) in vec3 pos;
layout (location = 1) in vec2 tex;
layout (location = 2) in vec3 norm;
layout (location = 3) in ivec4 boneIds;
layout (location = 4) in vec4 boneWeights;
layout (location = 5) in vec3 col;

out vec4 vCol;
out vec2 TexCoord;

uniform mat4 model;
uniform mat4 projection;
uniform mat4 view;

const int MAX_BONES = 100;
uniform mat4 gBones[MAX_BONES];
uniform bool useBones;

void main()
{
    vec4 totalPosition = vec4(0.0f);
    
    if (useBones) {
        float totalWeight = 0.0f;
        for (int i = 0; i < 4; i++) {
            if (boneWeights[i] <= 0.0f) continue;
            if (boneIds[i] < 0 || boneIds[i] >= MAX_BONES) continue;
            totalPosition += gBones[boneIds[i]] * vec4(pos, 1.0f) * boneWeights[i];
            totalWeight += boneWeights[i];
        }
        if (totalWeight <= 0.0f) {
            totalPosition = vec4(pos, 1.0f);
        }
    } else {
        totalPosition = vec4(pos, 1.0f);
    }
    
    gl_Position = projection * view * model * totalPosition;
    
    // Pass vertex color (used if no texture)
    // Fall back to position-based color when vertex colors are unset (zero)
    if (length(col) > 0.001) {
        vCol = vec4(col, 1.0f);
    } else {
        vCol = vec4(0.7f, 0.7f, 0.7f, 1.0f);
    }
    
    // Pass texture coordinates
    TexCoord = tex;
}

