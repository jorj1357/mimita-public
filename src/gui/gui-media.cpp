// C:\important\mimita-priv-v8\src\gui\gui-media.cpp
// 2026-06-13
// Purpose: Unified media rendering for GUI (PNG, JPG, GIF, future MP4)
// Each file should expose one main concept — this file exposes GUIMedia.

#include "gui-media.h"
#include "map/texture.h"
#include "stb_image.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <unordered_map>
#include <vector>

static std::unordered_map<std::string, GifCache> gGifCache;

static bool hasExtension(const std::string& path, const char* ext) {
    size_t pos = path.rfind('.');
    if (pos == std::string::npos) return false;
    std::string e = path.substr(pos);
    std::transform(e.begin(), e.end(), e.begin(), ::tolower);
    return e == ext;
}

// Check if file has been modified since last check
static bool fileChanged(const std::string& path, std::filesystem::file_time_type& lastWrite) {
    std::error_code ec;
    auto wt = std::filesystem::last_write_time(path, ec);
    if (ec) return false;
    if (wt != lastWrite) { lastWrite = wt; return true; }
    return false;
}

static GLuint uploadFrame(const unsigned char* data, int w, int h) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    return tex;
}

GLuint loadMediaTexture(const char* path, int* outW, int* outH) {
    if (!path || !path[0]) return 0;
    if (outW) *outW = 0;
    if (outH) *outH = 0;

    int w, h, n;
    unsigned char* data = stbi_load(path, &w, &h, &n, 4);
    if (!data) {
        printf("[MEDIA] Failed to load: %s\n", path);
        return 0;
    }
    if (outW) *outW = w;
    if (outH) *outH = h;
    GLuint tex = uploadFrame(data, w, h);
    stbi_image_free(data);
    return tex;
}

const GifCache* loadGif(const char* path) {
    if (!path || !path[0]) return nullptr;

    // Check cache
    auto it = gGifCache.find(path);
    if (it != gGifCache.end()) {
        // Check hot reload
        std::error_code ec;
        auto wt = std::filesystem::last_write_time(path, ec);
        if (!ec && wt != it->second.fileWriteTime) {
            // File changed — release old textures and reload
            for (GLuint t : it->second.frames) {
                if (t) glDeleteTextures(1, &t);
            }
            gGifCache.erase(it);
        } else {
            return &it->second;
        }
    }

    // Load GIF frames
    int w, h, n, frameCount;
    int* delays = nullptr;
    unsigned char* gifData = nullptr;
    size_t fileSize = 0;

    FILE* f = fopen(path, "rb");
    if (!f) { printf("[MEDIA] Cannot open GIF: %s\n", path); return nullptr; }
    fseek(f, 0, SEEK_END); fileSize = (size_t)ftell(f); fseek(f, 0, SEEK_SET);
    std::vector<unsigned char> buf(fileSize);
    fread(buf.data(), 1, fileSize, f);
    fclose(f);

    unsigned char* frames = stbi_load_gif_from_memory(
        buf.data(), (int)fileSize, &delays, &w, &h, &n, &frameCount, 4);

    if (!frames || frameCount <= 0) {
        printf("[MEDIA] Failed to load GIF: %s\n", path);
        stbi_image_free(frames);
        return nullptr;
    }

    GifCache cache;
    cache.width = w;
    cache.height = h;
    cache.frameCount = frameCount;
    cache.currentFrame = 0;
    cache.timer = 0.0f;
    std::error_code ec;
    cache.fileWriteTime = std::filesystem::last_write_time(path, ec);

    int frameSize = w * h * 4;
    for (int i = 0; i < frameCount; ++i) {
        GLuint tex = uploadFrame(frames + i * frameSize, w, h);
        cache.frames.push_back(tex);
        cache.delays.push_back(delays ? delays[i] : 10);
    }

    stbi_image_free(frames);
    stbi_image_free(delays);

    auto result = gGifCache.emplace(path, std::move(cache));
    printf("[MEDIA] Loaded GIF: %s (%d frames %dx%d)\n", path, frameCount, w, h);
    return &result.first->second;
}

void updateGifFrame(GifCache* gif, float dt) {
    if (!gif || gif->frameCount <= 1) return;
    gif->timer += dt * 1000.0f; // convert seconds to ms
    if (gif->delays[gif->currentFrame] > 0 &&
        gif->timer >= (float)gif->delays[gif->currentFrame]) {
        gif->timer = 0.0f;
        gif->currentFrame = (gif->currentFrame + 1) % gif->frameCount;
    }
}

void updateAllGifs(float dt) {
    for (auto& pair : gGifCache) {
        updateGifFrame(&pair.second, dt);
    }
}

void clearMediaCache() {
    for (auto& pair : gGifCache) {
        for (GLuint t : pair.second.frames) {
            if (t) glDeleteTextures(1, &t);
        }
    }
    gGifCache.clear();
}
