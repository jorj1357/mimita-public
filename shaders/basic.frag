// C:\important\quiet\n\mimita-priv-v7\shaders\basic.frag
// mar 6 2026
// basic shaders fragments? not sure 

#version 330 core
out vec4 FragColor;

in vec2 vUV;
in float vDepth;

uniform sampler2D uTex;
uniform int uUseColor;
uniform vec4 uColor;

void main() {
    if (uUseColor == 1) {
        FragColor = uColor;
        return;
    }

    vec4 texColor = texture(uTex, vUV);

    float ambient = 0.5;
    float depthShade = clamp(1.0 - vDepth * 0.002, 0.5, 1.0);
    float shade = max(ambient, depthShade);

    FragColor = vec4(texColor.rgb * shade, texColor.a);
}
