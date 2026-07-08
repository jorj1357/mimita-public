#version 330 core

layout (location = 0) in vec3 aPos;

uniform mat4 view;
uniform mat4 projection;
uniform float uGlobalRotation;

out vec3 vUVW;

mat4 rotateY(float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return mat4(
        c, 0, s, 0,
        0, 1, 0, 0,
        -s, 0, c, 0,
        0, 0, 0, 1
    );
}

void main() {
    vec4 pos = vec4(aPos, 1.0);
    // Apply global Y-axis rotation
    pos = rotateY(radians(uGlobalRotation)) * pos;
    // Remove translation from view matrix so skybox follows camera
    mat4 viewNoTrans = mat4(mat3(view));
    gl_Position = projection * viewNoTrans * pos;
    vUVW = aPos;
}
