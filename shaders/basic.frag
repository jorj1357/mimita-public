// MiMITA stylized gameplay shader.
//
// This is not a realistic PBR shader. It is designed for movement readability:
// - ambient keeps dark areas playable
// - directional diffuse separates floor/walls/slopes
// - fresnel edge darkening gives silhouettes and tactile geometry
// - fake AO/cavity darkening makes corners and grazing surfaces easier to read
// - debug modes expose UVs, normals, textures, lighting, and AO directly

#version 330 core

out vec4 FragColor;

in vec2 vUV;
in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vViewDir;
in vec4 vDebugColor;

uniform sampler2D uTex;

// Debug color path used by world debug line overlays.
uniform int  uUseColor;
uniform vec4 uColor;

// Per-object tint multiplier applied to textured output.
// Set to (1,1,1) by default; weapons and future items override for color tinting.
uniform vec3 uTint;

// Debug mode:
// 0 normal stylized render
// 1 UV checkerboard
// 2 lighting only
// 3 texture only
// 4 fake AO only
// 5 normals as color
uniform int uDebugView;

// Directional light. Think of this as a fixed readable sun, not realism.
uniform vec3 uLightDir;

// Higher = brighter dark areas. Useful for gameplay readability.
uniform float uAmbientStrength;

// Higher = stronger facing-light surfaces. Lower = flatter PS2-like look.
uniform float uDiffuseStrength;

// Higher = darker silhouettes and grazing-angle surfaces.
uniform float uEdgeDarkness;

// Higher = edge effect expands inward from silhouette.
uniform float uEdgeWidth;

// Higher = darker fake contact/cavity shading.
uniform float uAODarkness;

// Higher = sharper AO contrast. Lower = smoother/softer.
uniform float uAOContrast;

// Higher = stronger texture colors. Useful for stylized readability.
uniform float uTextureContrast;

// Higher = brighter texture before lighting.
uniform float uTextureBrightness;

// Shadow mapping
uniform sampler2D uShadowMap;
uniform mat4 uShadowMatrix;
uniform float uShadowDarkness;
uniform float uShadowBias;
uniform float uShadowSoftness;
uniform vec3 uShadowTint;
uniform bool uShadowsEnabled;

in vec4 vShadowCoord;

vec3 applyTextureTuning(vec3 color)
{
    // Contrast around 0.5 keeps texture art readable without needing PBR.
    color = (color - 0.5) * uTextureContrast + 0.5;
    color *= uTextureBrightness;
    return clamp(color, 0.0, 1.0);
}

float checker(vec2 uv)
{
    // UV debug: if UVs are wrong, this checker stretches, smears, or disappears.
    vec2 c = floor(fract(uv * 12.0) * 2.0);
    return mod(c.x + c.y, 2.0);
}

float sampleShadow(vec3 sc, float bias, float softness)
{
    ivec2 mapSize = textureSize(uShadowMap, 0);
    vec2 texel = 1.0 / vec2(mapSize);
    float radius = softness;
    float s = 0.0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            vec2 offset = vec2(x, y) * texel * radius;
            float d = texture(uShadowMap, sc.xy + offset).r;
            s += (sc.z - bias) > d ? 1.0 : 0.0;
        }
    }
    return s / 9.0;
}

void main()
{
    if (uUseColor == 1) {
        FragColor = uColor;
        return;
    }
    if (uUseColor == 2) {
        FragColor = vDebugColor;
        return;
    }

    vec3 N = normalize(vNormal);
    vec3 V = normalize(vViewDir);
    vec3 L = normalize(-uLightDir);

    // Texture sampling:
    // UVs address a 2D image. Mipmaps selected by OpenGL reduce shimmer at distance.
    // The C++ texture loader controls filtering/wrapping; this shader just samples.
    vec4 texel = texture(uTex, vUV);
    vec3 texColor = applyTextureTuning(texel.rgb) * uTint;

    // Dot product lighting:
    // dot(N, L)=1 means the surface faces the light.
    // dot(N, L)=0 means it is perpendicular and should be darker.
    float ndotl = max(dot(N, L), 0.0);
    float diffuse = ndotl * uDiffuseStrength;

    // Fresnel/edge term:
    // dot(N,V) is low near silhouettes/grazing angles. Darkening those areas makes
    // object outlines easier to read during fast motion.
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), max(uEdgeWidth, 0.001));
    float edgeShade = 1.0 - fresnel * uEdgeDarkness;

    // Fake AO:
    // Cheap global approximation. Up-facing/open surfaces stay brighter, side/down
    // surfaces get a small cavity-like darkening. This is not physically correct;
    // it is here to separate surfaces and make the world more tactile.
    float upward = clamp(N.z * 0.5 + 0.5, 0.0, 1.0);
    float fakeAO = pow(1.0 - upward, max(uAOContrast, 0.001)) * uAODarkness;
    float aoShade = 1.0 - fakeAO;

    float light = uAmbientStrength + diffuse;
    vec3 lit = texColor * light * edgeShade * aoShade;

    // Shadow
    float shadow = 0.0;
    if (uShadowsEnabled) {
        vec3 sc = vShadowCoord.xyz / vShadowCoord.w;
        if (sc.x >= 0.0 && sc.x <= 1.0 && sc.y >= 0.0 && sc.y <= 1.0 && sc.z <= 1.0) {
            shadow = sampleShadow(sc, uShadowBias, uShadowSoftness);
        }
        float shadowFactor = 1.0 - shadow * uShadowDarkness;
        lit = lit * shadowFactor + uShadowTint * shadow * uShadowDarkness;
    }

    if (uDebugView == 1) {
        float c = checker(vUV);
        FragColor = vec4(mix(vec3(0.05, 0.15, 0.25), vec3(1.0, 0.8, 0.15), c), 1.0);
    } else if (uDebugView == 2) {
        FragColor = vec4(vec3(light * edgeShade), 1.0);
    } else if (uDebugView == 3) {
        FragColor = vec4(texColor, 1.0);
    } else if (uDebugView == 4) {
        FragColor = vec4(vec3(aoShade), 1.0);
    } else if (uDebugView == 5) {
        FragColor = vec4(N * 0.5 + 0.5, 1.0);
    } else {
        FragColor = vec4(lit, texel.a);
    }
}
