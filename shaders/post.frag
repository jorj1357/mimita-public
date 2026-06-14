// Full-screen post-processing pass.
// Applied after the entire scene + UI has been rendered.

#version 330 core

out vec4 FragColor;

in vec2 vUV;

uniform sampler2D uScene;

uniform float uBrightness;
uniform float uContrast;
uniform float uSaturation;
uniform float uGamma;
uniform float uHueShift;

vec3 hueShift(vec3 color, float shift)
{
    const vec3 k = vec3(0.57735, 0.57735, 0.57735);
    float cosA = cos(shift);
    float sinA = sin(shift);
    return color * cosA + cross(k, color) * sinA + k * dot(k, color) * (1.0 - cosA);
}

void main()
{
    vec3 color = texture(uScene, vUV).rgb;

    // Brightness
    color *= uBrightness;

    // Contrast
    color = (color - 0.5) * uContrast + 0.5;

    // Saturation
    float luma = dot(color, vec3(0.299, 0.587, 0.114));
    color = mix(vec3(luma), color, uSaturation);

    // Hue shift
    color = hueShift(color, uHueShift);

    // Gamma correction
    color = pow(clamp(color, 0.0, 1.0), vec3(1.0 / uGamma));

    FragColor = vec4(color, 1.0);
}
