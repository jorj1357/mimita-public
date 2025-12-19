#version 330 core

in vec2 vUV;
flat in float vTex;

out vec4 FragColor;

uniform sampler2D uTextures[16];

void main() {
    FragColor = texture(uTextures[int(vTex)], vUV);
}
