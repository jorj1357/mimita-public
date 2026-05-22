// C:\important\quiet\n\mimita-priv-v7\shaders\basic.vert
// mar 6 2026
// basic shaders vertices

#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 vUV;
out float vDepth;

void main() {
    vec4 worldPos = model * vec4(aPos, 1.0);

    vUV = aUV;
    vDepth = worldPos.z;

    gl_Position = projection * view * worldPos;
}
