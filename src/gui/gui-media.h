// C:\important\mimita-priv-v8\src\gui\gui-media.h
// 2026-06-13
// Purpose: Unified media rendering for GUI elements.
// Supports PNG, JPG, GIF. MP4 stub for future use.

#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <filesystem>
#include <glad/glad.h>

// Cached animated GIF
struct GifCache {
    int width = 0;
    int height = 0;
    int frameCount = 0;
    int currentFrame = 0;
    float timer = 0.0f;
    std::vector<GLuint> frames;   // one GL texture per frame
    std::vector<int> delays;      // frame delay in ms
    std::filesystem::file_time_type fileWriteTime;
};

// Load a static image (PNG/JPG). Returns OpenGL texture ID, sets outW/outH.
GLuint loadMediaTexture(const char* path, int* outW = nullptr, int* outH = nullptr);

// Load an animated GIF (cached). Returns pointer to GifCache or nullptr.
const GifCache* loadGif(const char* path);

// Advance GIF frame based on real time. Call once per frame.
void updateGifFrame(GifCache* gif, float dt);

// Clear all cached GIF textures.
void clearMediaCache();

// Update all animated GIFs. Call once per frame.
void updateAllGifs(float dt);
