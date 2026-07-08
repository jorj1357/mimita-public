#version 330 core

in vec3 vUVW;

uniform samplerCube uCubeMap;

uniform vec3  uFaceColor[6];
uniform float uFaceAlpha[6];
uniform float uFaceHue[6];
uniform vec2  uFaceStretch[6];
uniform vec2  uFaceUVOffset[6];

uniform float uGlobalHue;
uniform vec2  uGlobalScale;

out vec4 FragColor;

// Convert RGB to HSV
vec3 rgb2hsv(vec3 col) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(col.bg, K.wz), vec4(col.gb, K.xy), step(col.b, col.g));
    vec4 q = mix(vec4(p.xyw, col.r), vec4(col.r, p.yzx), step(p.x, col.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

// Convert HSV to RGB
vec3 hsv2rgb(vec3 col) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 hue = vec3(col.x);
    vec3 p = abs(fract(hue + K.xyz) * 6.0 - K.www);
    vec3 one = vec3(1.0);
    return col.z * mix(one, clamp(p - one, 0.0, 1.0), col.y);
}

// Determine which cubemap face a direction vector hits (for per-face effects)
int faceIndex(vec3 dir) {
    vec3 ad = abs(dir);
    if (ad.x >= ad.y && ad.x >= ad.z) return dir.x >= 0.0 ? 0 : 1; // +X/-X
    if (ad.y >= ad.x && ad.y >= ad.z) return dir.y >= 0.0 ? 2 : 3; // +Y/-Y
    return dir.z >= 0.0 ? 4 : 5; // +Z/-Z
}

// Apply per-face animated stretch to UVW
vec3 stretchUVW(vec3 dir, vec2 stretch, vec2 offset) {
    // Stretch is applied per axis (breathe effect), offset scrolls the texture
    return dir * vec3(1.0 / max(stretch.x, 0.01), 1.0 / max(stretch.y, 0.01), 1.0)
         + vec3(offset, 0.0);
}

void main() {
    vec3 dir = normalize(vUVW);
    dir.x *= uGlobalScale.x;
    dir.y *= uGlobalScale.y;

    int fi = faceIndex(dir);
    vec3 stretched = stretchUVW(dir, uFaceStretch[fi], uFaceUVOffset[fi]);

    vec4 texColor = texture(uCubeMap, stretched);

    // Per-face color tint
    texColor.rgb *= uFaceColor[fi];

    // Per-face hue shift
    vec3 hsv = rgb2hsv(texColor.rgb);
    hsv.x += uFaceHue[fi] + uGlobalHue;
    texColor.rgb = hsv2rgb(hsv);

    // Per-face alpha
    texColor.a *= uFaceAlpha[fi];

    FragColor = texColor;
}
