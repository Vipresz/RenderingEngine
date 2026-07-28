#version 330 core

layout (location = 0) in vec3 pos;
layout (location = 3) in ivec4 boneIDs;
layout (location = 4) in vec4 boneWeights;

uniform mat4 model;
uniform mat4 lightSpaceMatrix;

#define MAX_BONES 100
uniform mat4 boneTransforms[MAX_BONES];
uniform int boneCount;

void main()
{
    vec4 finalPosition = vec4(pos, 1.0);
    if (boneCount > 0) {
        mat4 boneTransform = mat4(0.0);
        if (boneIDs.x >= 0 && boneIDs.x < MAX_BONES) {
            boneTransform += boneTransforms[boneIDs.x] * boneWeights.x;
        }
        if (boneIDs.y >= 0 && boneIDs.y < MAX_BONES) {
            boneTransform += boneTransforms[boneIDs.y] * boneWeights.y;
        }
        if (boneIDs.z >= 0 && boneIDs.z < MAX_BONES) {
            boneTransform += boneTransforms[boneIDs.z] * boneWeights.z;
        }
        if (boneIDs.w >= 0 && boneIDs.w < MAX_BONES) {
            boneTransform += boneTransforms[boneIDs.w] * boneWeights.w;
        }
        finalPosition = boneTransform * finalPosition;
    }

    gl_Position = lightSpaceMatrix * model * finalPosition;
}
