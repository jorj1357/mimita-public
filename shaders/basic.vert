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

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 vUV;
out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vViewDir;

void main() {
    vec4 worldPos = model * vec4(aPos, 1.0);

    vUV = aUV;
    vWorldPos = worldPos.xyz;

    // Normals should not use the regular model matrix when scale is non-uniform.
    // The inverse-transpose normal matrix keeps lighting stable when artists scale meshes.
    vNormal = normalize(mat3(transpose(inverse(model))) * aNormal);

    // Camera-space direction is useful for fresnel/edge darkening in the fragment shader.
    vec3 cameraPos = inverse(view)[3].xyz;
    vViewDir = normalize(cameraPos - worldPos.xyz);

    gl_Position = projection * view * worldPos;
}
