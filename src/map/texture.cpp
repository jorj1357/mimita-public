// C:\important\quiet\n\mimita-public\mimita-public\src\map\texture.cpp
// dec 19 2025 need update i think 


// todo use this texture or use texture_manager.cpp

// jan 25 2026 
// dont include stb image implementation here? i dont know -jorj 

#include <glad/glad.h>
#include "game/gl-resource-bridge-module.h"
/*
dont use this
#include <GL/glew.h>   // or <GL/gl.h> if using raw OpenGL
its bad and breaks everythung
*/
#include <cstdio>

#include "stb_image.h"
#include "utils/path_utils.h"
#include "debug/gl-debug.h"
#include "render/render-command.h"
#include <string>

namespace r = mimita::game::render;

static std::uint32_t makeFallbackTexture()
{
    static std::uint32_t fallback = 0;
    if (fallback)
        return fallback;

    printf("[TEXTURE WARNING] Creating bright checkerboard fallback texture\n");
    unsigned int pixels[16] = {
        0xffff00ff, 0xff00ffff, 0xffff00ff, 0xff00ffff,
        0xff00ffff, 0xffff00ff, 0xff00ffff, 0xffff00ff,
        0xffff00ff, 0xff00ffff, 0xffff00ff, 0xff00ffff,
        0xff00ffff, 0xffff00ff, 0xff00ffff, 0xffff00ff
    };

    MIMITA_GL_CLEAR_STAGE("makeFallbackTexture");
    fallback = mimita::game::genTexture();
    r::bindTexture(r::TextureTarget::Texture2D, fallback);
    r::texImage2D(r::TextureTarget::Texture2D, 0, r::TextureFormat::RGBA,
                  4, 4, 0, r::PixelFormat::RGBA, r::PixelType::UnsignedByte, pixels);
    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::WrapS,
                     r::TextureValue::Repeat);
    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::WrapT,
                     r::TextureValue::Repeat);
    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::MinFilter,
                     r::TextureValue::Nearest);
    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::MagFilter,
                     r::TextureValue::Nearest);
    return fallback;
}

std::uint32_t loadTexture(const char* path) {
    printf("[TEXTURE] Loading texture %s\n", path);

    std::string resolvedPath = path;
    int w, h, n;
    unsigned char* data = stbi_load(resolvedPath.c_str(), &w, &h, &n, 4);

    if (!data || w <= 0 || h <= 0) {
        printf("[TEXTURE WARNING] Missing texture %s (resolved from %s)\n", resolvedPath.c_str(), path);
        if (data)
            stbi_image_free(data);
        return makeFallbackTexture();
    }

    std::uint32_t tex;
    MIMITA_GL_CLEAR_STAGE("loadTexture");
    tex = mimita::game::genTexture();
    if (!tex)
    {
        stbi_image_free(data);
        return makeFallbackTexture();
    }
    r::bindTexture(r::TextureTarget::Texture2D, tex);

    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::WrapS,
                     r::TextureValue::Repeat);
    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::WrapT,
                     r::TextureValue::Repeat);
    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::MinFilter,
                     r::TextureValue::LinearMipmapLinear);
    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::MagFilter,
                     r::TextureValue::Nearest);

    printf("[TEXTURE] Uploading to GPU tex=%u size=%dx%d channels=%d\n", tex, w, h, n);
    r::pixelStorei(r::PixelStoreParameter::UnpackAlignment, 1);
    r::texImage2D(r::TextureTarget::Texture2D, 0, r::TextureFormat::RGBA,
                  w, h, 0, r::PixelFormat::RGBA, r::PixelType::UnsignedByte, data);
    r::generateMipmap(r::TextureTarget::Texture2D);
    r::pixelStorei(r::PixelStoreParameter::UnpackAlignment, 4);

    stbi_image_free(data);
    printf("[TEXTURE] Upload complete %s tex=%u\n", path, tex);

    return tex;
}
