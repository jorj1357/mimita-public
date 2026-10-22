// C:\important\quiet\n\mimita-priv-v7\shaders\basic.vert
// mar 6 2026
// basic shader vertex stage
//
// This shader is intentionally simple and educational:
// - aPos is the world/model-space vertex position from GLB/OBJ/procedural meshes.
// - aUV is the texture coordinate. UVs map a 2D image onto the 3D triangle.
// - aNormal tells the fragment shader which way a surface faces for lighting.
//
// Attribute locations must match the C++ VAO setup:
// location 0 = position, location 1 = UV, location 2 = normal.

#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec4 aDebugColor;
// Combined player-body meshes carry the source part index per vertex. The
// matching part matrix is supplied once per actor instead of one draw per part.
layout (location = 8) in float aPartIndex;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 uShadowMatrix;
uniform int uOutlinePass;
uniform float uOutlineThickness;
uniform int uUsePartModels;
uniform mat4 uPartModels[32];

out vec2 vUV;
out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vViewDir;
out vec4 vDebugColor;
out vec4 vShadowCoord;

void main() {
    mat4 modelMatrix = model;
    if (uUsePartModels != 0) {
        int partIndex = int(aPartIndex + 0.5);
        if (partIndex >= 0 && partIndex < 32)
            modelMatrix = uPartModels[partIndex];
    }

    vec4 worldPos = modelMatrix * vec4(aPos, 1.0);
    if (uOutlinePass != 0)
        worldPos.xyz += normalize(mat3(transpose(inverse(modelMatrix))) * aNormal) * uOutlineThickness;

    vUV = aUV;
    vWorldPos = worldPos.xyz;

    // Normals should not use the regular model matrix when scale is non-uniform.
    // The inverse-transpose normal matrix keeps lighting stable when artists scale meshes.
    vNormal = normalize(mat3(transpose(inverse(modelMatrix))) * aNormal);
    vDebugColor = aDebugColor;

    // Camera-space direction is useful for fresnel/edge darkening in the fragment shader.
    vec3 cameraPos = inverse(view)[3].xyz;
    vViewDir = normalize(cameraPos - worldPos.xyz);

    vShadowCoord = uShadowMatrix * worldPos;

    gl_Position = projection * view * worldPos;
}
