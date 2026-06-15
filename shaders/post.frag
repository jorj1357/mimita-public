// Full-screen post-processing pass with all visual effects.
// Every uniform defaults to 0.0 (off) — no wasted GPU time for disabled effects.

#version 330 core

out vec4 FragColor;
in vec2 vUV;

uniform sampler2D uScene;

// Core
uniform float uBrightness;
uniform float uContrast;
uniform float uSaturation;
uniform float uGamma;
uniform float uHueShift;
uniform float uColorTemperature;
uniform float uVignette;
uniform float uFilmGrain;
uniform float uChromaticAberration;
uniform float uLensDistortion;
uniform float uScanlines;
uniform float uPixelation;
uniform float uPosterize;

// Artistic modes
uniform float uDreamStrength;
uniform float uVoidStrength;
uniform float uPsychedelicStrength;
uniform float uRetroStrength;
uniform float uGlitchStrength;

// Procedural modifiers
uniform float uWorldWave;
uniform float uScreenWave;
uniform float uScreenShakeFx;
uniform float uEdgeGlow;
uniform float uOutlineBoost;
uniform float uShadowBoost;

// Screen info + time
uniform float uTime;
uniform float uScreenW;
uniform float uScreenH;

// ------ Utility functions ------

vec3 hueShift(vec3 color, float shift)
{
    const vec3 k = vec3(0.57735, 0.57735, 0.57735);
    float cosA = cos(shift);
    float sinA = sin(shift);
    return color * cosA + cross(k, color) * sinA + k * dot(k, color) * (1.0 - cosA);
}

float random(vec2 st)
{
    return fract(sin(dot(st.xy, vec2(12.9898,78.233))) * 43758.5453);
}

// Nonlinear scaling: low values = subtle, high values = extreme
float nls(float x, float power)
{
    return pow(clamp(x, 0.0, 1.0), power);
}

vec2 applyLensDistortion(vec2 uv, float strength)
{
    float dist = dot(uv - 0.5, uv - 0.5);
    return uv + (uv - 0.5) * dist * strength * 0.5;
}

// ------ Main ------

void main()
{
    vec2 uv = vUV;
    vec2 fragCoord = uv * vec2(uScreenW, uScreenH);
    vec3 col = texture(uScene, uv).rgb;
    float strength = 0.0;

    // --- Screen wave (entire frame distortion) ---
    strength = uScreenWave;
    if (strength > 0.001)
    {
        float wave = sin(uv.y * 60.0 * strength + uTime * 3.0) * strength * 0.02;
        uv.x += wave;
        wave = sin(uv.x * 60.0 * strength + uTime * 2.5) * strength * 0.02;
        uv.y += wave;
        col = texture(uScene, uv).rgb;
    }

    // --- Chromatic aberration (before distortion to avoid double-warp) ---
    strength = abs(uChromaticAberration);
    if (strength > 0.001)
    {
        float off = strength * 0.02;
        float r = texture(uScene, uv + vec2(off, 0.0)).r;
        float b = texture(uScene, uv - vec2(off, 0.0)).b;
        col.r = mix(col.r, r, 1.0);
        col.b = mix(col.b, b, 1.0);
    }

    // --- Lens distortion ---
    strength = uLensDistortion;
    if (abs(strength) > 0.001)
    {
        vec2 duv = applyLensDistortion(uv, strength);
        col = texture(uScene, duv).rgb;
    }

    // --- Glitch (RGB split + displacement) ---
    strength = nls(uGlitchStrength, 2.0);
    if (strength > 0.001)
    {
        float glitchAmount = strength * 0.04;
        float glitchLine = step(0.99 - strength * 0.3, random(vec2(floor(fragCoord.y * 0.5), uTime * 0.5)));
        float offset = (random(vec2(floor(uTime * 10.0), floor(fragCoord.y * 2.0))) - 0.5) * glitchAmount;
        if (glitchLine > 0.5)
        {
            col.r = texture(uScene, uv + vec2(offset, 0.0)).r;
            col.b = texture(uScene, uv - vec2(offset * 0.5, 0.0)).b;
        }
    }

    // --- Brightness ---
    col *= uBrightness;

    // --- Contrast ---
    col = (col - 0.5) * uContrast + 0.5;

    // --- Saturation ---
    float luma = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(vec3(luma), col, uSaturation);

    // --- Hue shift ---
    col = hueShift(col, uHueShift);

    // --- Color temperature ---
    strength = uColorTemperature;
    if (abs(strength) > 0.001)
    {
        col.r *= 1.0 + strength * 0.15;
        col.b *= 1.0 - strength * 0.15;
    }

    // --- Posterize (reduce color levels) ---
    strength = nls(uPosterize, 1.5);
    if (strength > 0.001)
    {
        float levels = max(2.0, 256.0 - strength * 250.0);
        col = floor(col * levels) / levels;
    }

    // --- Pixelation ---
    strength = nls(uPixelation, 1.5);
    if (strength > 0.001)
    {
        float cellSize = max(1.0, strength * 50.0);
        vec2 cell = floor(uv * vec2(uScreenW, uScreenH) / cellSize) * cellSize / vec2(uScreenW, uScreenH);
        col = texture(uScene, cell).rgb;
    }

    // --- Void (fade edges + desaturate) ---
    strength = nls(uVoidStrength, 2.0);
    if (strength > 0.001)
    {
        float dist = distance(uv, vec2(0.5));
        float falloff = 1.0 - dist * strength * 1.5;
        col *= max(0.0, falloff);
        col = mix(col, vec3(dot(col, vec3(0.3, 0.3, 0.3))), strength * 0.5);
    }

    // --- Vignette ---
    strength = uVignette;
    if (strength > 0.001)
    {
        float dist = distance(uv, vec2(0.5));
        float vignette = 1.0 - dist * dist * strength * 2.0;
        col *= clamp(vignette, 0.0, 1.0);
    }

    // --- Film grain ---
    strength = uFilmGrain;
    if (strength > 0.001)
    {
        float grain = random(uv * uTime * 100.0) * strength * 0.15;
        col += grain;
    }

    // --- Scanlines ---
    strength = nls(uScanlines, 1.5);
    if (strength > 0.001)
    {
        float scanline = sin(fragCoord.y * 3.14159 * 2.0) * strength * 0.15;
        col -= scanline;
    }

    // --- Dream (soft blur + slight warping) ---
    strength = nls(uDreamStrength, 2.0);
    if (strength > 0.001)
    {
        vec2 dreamOff = vec2(sin(uv.y * 30.0 + uTime) * strength * 0.005,
                             cos(uv.x * 30.0 + uTime * 0.7) * strength * 0.005);
        vec3 dreamCol = texture(uScene, uv + dreamOff).rgb;
        dreamCol += texture(uScene, uv - dreamOff).rgb;
        dreamCol *= 0.5;
        col = mix(col, dreamCol, strength * 0.5);
        col += vec3(0.02, 0.01, 0.03) * strength * 0.1;
    }

    // --- Psychedelic (hue cycling + rainbow distortion) ---
    strength = nls(uPsychedelicStrength, 2.0);
    if (strength > 0.001)
    {
        float hueCycle = uTime * 0.3 * strength;
        vec3 rainbow = hueShift(col, hueCycle + uv.x * strength * 3.0 + uv.y * strength * 3.0);
        col = mix(col, rainbow, strength * 0.6);
        float pulse = sin(uTime * 2.0 + uv.x * 10.0 * strength) * 0.5 + 0.5;
        col *= 1.0 + pulse * strength * 0.2;
    }

    // --- Retro (color banding + reduced precision) ---
    strength = nls(uRetroStrength, 2.0);
    if (strength > 0.001)
    {
        float levels = max(4.0, 32.0 - strength * 28.0);
        col = floor(col * levels) / levels;
    }

    // --- Gamma correction (always applied) ---
    col = pow(clamp(col, 0.0, 1.0), vec3(1.0 / uGamma));

    // --- Edge glow (post-gamma for consistent brightness) ---
    strength = uEdgeGlow;
    if (strength > 0.001)
    {
        vec2 off = vec2(1.0 / uScreenW, 1.0 / uScreenH) * 2.0;
        float edge = 0.0;
        for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++)
            edge += length(texture(uScene, uv + vec2(x, y) * off).rgb);
        edge = abs(edge / 9.0 - length(col));
        col += vec3(edge * strength * 0.5);
    }

    FragColor = vec4(col, 1.0);
}
