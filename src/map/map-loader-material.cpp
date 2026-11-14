#include "map-loader-material.h"
#include "game/gl-resource-bridge-module.h"
#include "map_loader.h"
#include "tinygltf/tiny_gltf.h"

#include <cstdio>
#include <string>
#include <vector>

#include <glad/glad.h>

#include "world/texture-store.h"
#include "utils/path_utils.h"
#include "debug/debug-log.h"
#include "render/render-command.h"

namespace r = mimita::game::render;

extern TextureStore gTextures;

namespace {

#define GLB_LOG(...) Debug::logAuto(Debug::Category::GLB, __VA_ARGS__)

std::uint32_t loadExternalImage(const std::string& uri, const std::string& glbDir)
{
    printf("[TEXTURE SEARCH] trying external URI: %s\n", uri.c_str());

    std::string tryPath = resolveAssetPath(uri);
    if (!tryPath.empty())
    {
        std::uint32_t tex = gTextures.getPath(tryPath, false);
        if (tex)
        {
            printf("[TEXTURE SEARCH] FOUND %s\n", tryPath.c_str());
            return tex;
        }
    }

    if (!glbDir.empty())
    {
        std::string relPath = glbDir + "/" + uri;
        printf("[TEXTURE SEARCH] trying %s\n", relPath.c_str());
        std::uint32_t tex = gTextures.getPath(relPath, false);
        if (tex)
        {
            printf("[TEXTURE SEARCH] FOUND %s\n", relPath.c_str());
            return tex;
        }
    }

    size_t slashPos = uri.find_last_of("/\\");
    std::string filename = (slashPos != std::string::npos) ? uri.substr(slashPos + 1) : uri;
    std::string texPath = "assets/textures/" + filename;
    printf("[TEXTURE SEARCH] trying %s\n", texPath.c_str());
    std::uint32_t tex = gTextures.getPath(texPath, false);
    if (tex)
    {
        printf("[TEXTURE SEARCH] FOUND %s\n", texPath.c_str());
        return tex;
    }

    size_t dotPos = filename.rfind('.');
    if (dotPos != std::string::npos)
    {
        std::string nameOnly = filename.substr(0, dotPos);
        printf("[TEXTURE SEARCH] trying %s\n", nameOnly.c_str());
        tex = gTextures.get(nameOnly);
        if (tex)
        {
            printf("[TEXTURE SEARCH] FOUND assets/textures/%s.png\n", nameOnly.c_str());
            return tex;
        }
    }

    printf("[TEXTURE SEARCH] NOT FOUND %s\n", uri.c_str());
    return 0;
}

std::uint32_t uploadGLBImage(const tinygltf::Image& image, int imageIndex, const std::string& glbDir)
{
    if (!image.uri.empty() && (image.image.empty() || image.width <= 0 || image.height <= 0))
    {
        printf("[GLB IMAGE] index=%d uri=%s external image; trying file load\n",
               imageIndex, image.uri.c_str());
        return loadExternalImage(image.uri, glbDir);
    }

    if (image.image.empty() || image.width <= 0 || image.height <= 0)
    {
        printf("[GLB TEXTURE WARNING] image %d invalid data size=%zu dims=%dx%d\n",
                imageIndex, image.image.size(), image.width, image.height);
        if (!image.uri.empty())
        {
            printf("[GLB TEXTURE WARNING] image %d has uri=%s; trying external load\n",
                   imageIndex, image.uri.c_str());
            return loadExternalImage(image.uri, glbDir);
        }
        return 0;
    }

    if (image.component < 1 || image.component > 4)
    {
        GLB_LOG("[GLB TEXTURE WARNING] image %d unsupported component count=%d\n",
                imageIndex, image.component);
        return 0;
    }

    const size_t expectedBytes = (size_t)image.width * (size_t)image.height * (size_t)image.component;
    if (expectedBytes == 0 || image.image.size() < expectedBytes)
    {
        GLB_LOG("[GLB TEXTURE WARNING] image %d pixel buffer too small bytes=%zu expected=%zu dims=%dx%d components=%d\n",
                imageIndex, image.image.size(), expectedBytes, image.width, image.height, image.component);
        return 0;
    }

    std::uint32_t tex = 0;
    tex = mimita::game::genTexture();
    if (!tex)
    {
        GLB_LOG("[GLB TEXTURE WARNING] texture create returned 0 for image %d\n", imageIndex);
        return 0;
    }
    r::bindTexture(r::TextureTarget::Texture2D, tex);

    r::PixelFormat srcFormat = r::PixelFormat::RGBA;
    if (image.component == 1) srcFormat = r::PixelFormat::Red;
    else if (image.component == 2) srcFormat = r::PixelFormat::RG;
    else if (image.component == 3) srcFormat = r::PixelFormat::RGB;
    else if (image.component == 4) srcFormat = r::PixelFormat::RGBA;

    GLB_LOG("[GLB] loaded texture image=%d name=%s size=%dx%d components=%d bytes=%zu tex=%u\n",
           imageIndex, image.name.c_str(), image.width, image.height, image.component, image.image.size(), tex);
    Debug::log(Debug::Category::GLB, "[TEXTURE] Uploading GLB image to GPU and generating mipmaps\n");

    r::pixelStorei(r::PixelStoreParameter::UnpackAlignment, 1);
    r::texImage2D(r::TextureTarget::Texture2D, 0, r::TextureFormat::RGBA,
                  image.width, image.height, 0, srcFormat,
                  r::PixelType::UnsignedByte, image.image.data());

    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::WrapS,
                     r::TextureValue::Repeat);
    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::WrapT,
                     r::TextureValue::Repeat);

    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::MinFilter,
                     r::TextureValue::LinearMipmapLinear);

    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::MagFilter,
                     r::TextureValue::Linear);

    r::generateMipmap(r::TextureTarget::Texture2D);
    Debug::log(Debug::Category::GLB, "[TEXTURE] mipmaps generated for GLB texture tex=%u\n", tex);

    r::textureAnisotropy(r::TextureTarget::Texture2D, 16.0f);
    Debug::log(Debug::Category::GLB,
               "[TEXTURE] anisotropic filtering requested for GLB texture\n");

    r::pixelStorei(r::PixelStoreParameter::UnpackAlignment, 4);
    return tex;
}

} // anonymous namespace

void processGLBMaterials(
    tinygltf::Model& model,
    const std::string& glbDir,
    std::vector<std::uint32_t>& imageTextures,
    std::vector<std::uint32_t>& materialTextures,
    std::vector<std::uint32_t>& colorTextures)
{
    for (size_t i = 0; i < model.images.size(); ++i)
    {
        const tinygltf::Image& img = model.images[i];
        bool embedded = img.bufferView >= 0;
        printf("[GLB IMAGE] index=%zu name=%s width=%d height=%d components=%d embedded=%s bv=%d\n",
               i, img.name.c_str(), img.width, img.height, img.component,
               embedded ? "yes" : "no", img.bufferView);
        imageTextures[i] = uploadGLBImage(img, (int)i, glbDir);
    }

    for (size_t i = 0; i < model.materials.size(); ++i)
    {
        const tinygltf::Material& mat = model.materials[i];

        int baseColorTexIndex = mat.pbrMetallicRoughness.baseColorTexture.index;
        const std::vector<double>& bcf = mat.pbrMetallicRoughness.baseColorFactor;
        printf("[GLB MATERIAL] material=%zu name=%s baseColorTexture=%d baseColorFactor=[%.2f,%.2f,%.2f,%.2f]\n",
               i, mat.name.c_str(), baseColorTexIndex,
               bcf.size() >= 4 ? bcf[0] : 1.0,
               bcf.size() >= 4 ? bcf[1] : 1.0,
               bcf.size() >= 4 ? bcf[2] : 1.0,
               bcf.size() >= 4 ? bcf[3] : 1.0);

        const auto& pbr = mat.pbrMetallicRoughness;
        int texIndex = pbr.baseColorTexture.index;

        if (texIndex >= 0 && texIndex < (int)model.textures.size())
        {
            int imageIndex = model.textures[texIndex].source;
            printf("[GLB MATERIAL] material=%zu textureIndex=%d sourceImage=%d\n",
                   i, texIndex, imageIndex);
            if (imageIndex >= 0 && imageIndex < (int)imageTextures.size())
            {
                materialTextures[i] = imageTextures[imageIndex];
            }
            else
            {
                printf("[GLB WARNING] material=%zu texture source image out of range image=%d images=%zu\n",
                       i, imageIndex, imageTextures.size());
            }
        }
        else if (texIndex >= 0)
        {
            printf("[GLB WARNING] material=%zu texture index out of range texture=%d textures=%zu\n",
                   i, texIndex, model.textures.size());
        }

        if (!materialTextures[i])
        {
            bool hasColorFactor = bcf.size() >= 3;
            if (hasColorFactor)
            {
                float r = (float)(bcf.size() >= 1 ? bcf[0] : 1.0);
                float g = (float)(bcf.size() >= 2 ? bcf[1] : 1.0);
                float b = (float)(bcf.size() >= 3 ? bcf[2] : 1.0);
                float a = (float)(bcf.size() >= 4 ? bcf[3] : 1.0);

                printf("[GLB MATERIAL] material=%zu (%s) no texture; generating 1x1 from baseColorFactor (%.2f,%.2f,%.2f,%.2f)\n",
                       i, mat.name.c_str(), r, g, b, a);

                std::uint32_t tex = 0;
                tex = mimita::game::genTexture();
                if (tex)
                {
                    r::bindTexture(r::TextureTarget::Texture2D, tex);
                    unsigned char pixels[4];
                    pixels[0] = (unsigned char)(r * 255.0f);
                    pixels[1] = (unsigned char)(g * 255.0f);
                    pixels[2] = (unsigned char)(b * 255.0f);
                    pixels[3] = (unsigned char)(a * 255.0f);
                    r::texImage2D(r::TextureTarget::Texture2D, 0, r::TextureFormat::RGBA,
                                  1, 1, 0, r::PixelFormat::RGBA,
                                  r::PixelType::UnsignedByte, pixels);
                    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::WrapS,
                                     r::TextureValue::Repeat);
                    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::WrapT,
                                     r::TextureValue::Repeat);
                    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::MinFilter,
                                     r::TextureValue::Linear);
                    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::MagFilter,
                                     r::TextureValue::Linear);
                    materialTextures[i] = tex;
                    colorTextures.push_back(tex);
                }
                else
                {
                    printf("[GLB WARNING] material=%zu failed to create solid color texture; using default.png\n", i);
                    materialTextures[i] = gTextures.get("default");
                }
            }
            else
            {
                printf("[GLB TEXTURE WARNING] material=%zu (%s) has no baseColor texture and no baseColorFactor; using default.png\n",
                       i, mat.name.c_str());
                materialTextures[i] = gTextures.get("default");
            }
        }

        // Check KHR_texture_transform
        const auto& bct = pbr.baseColorTexture;
        auto texExtIt = bct.extensions.find("KHR_texture_transform");
        if (texExtIt != bct.extensions.end())
        {
            const tinygltf::Value& extVal = texExtIt->second;
            printf("[GLB MATERIAL] material=%zu KHR_texture_transform present\n", i);
        }

        printf("[GLB MATERIAL] material=%zu name=%s texture=%u\n", i, mat.name.c_str(), materialTextures[i]);
    }
}
