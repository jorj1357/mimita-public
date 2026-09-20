// C:\important\quiet\n\mimita-priv-v7\src\renderer\renderer.cpp
// feb 10 2026 REFACTOR INTO BEING SMALL YAYYY

// purpose
// Renderer creates the OpenGL context and GLAD
// Renderer does NOT own world shaders
// Renderer only exposes a “draw” API
// Your basic.vert / basic.frag stay exactly how they are

#define GLFW_INCLUDE_NONE
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

// need camera here
#include "camera.h"
#include "config.h"
#include "renderer.h"
#include "stb_image.h"
#include "utils/path_utils.h"
#include "debug/gl-debug.h"
#include "debug/debug-log.h"
#include "hot-reload/game-api.h"
#include "project/presentation-resource.h"

static std::string readTextFileImpl(const char* path, bool verbose)
{
    std::string resolved = resolveAssetPath(path);
    if (verbose)
        printf("[RENDERER] opening file: %s\n", resolved.c_str());

    FILE* f = fopen(resolved.c_str(), "rb");
    if (!f) {
        if (verbose)
            printf("[RENDERER] fopen failed\n");
        return "";
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::string text;
    text.resize(size);

    fread(text.data(), 1, size, f);
    fclose(f);

    if (verbose)
        printf("[RENDERER] loaded %ld bytes\n", size);

    return text;
}

static std::string readTextFile(const char* path)
{
    return readTextFileImpl(path, true);
}

// Quiet read used by per-frame generation polling (no console spam).
static std::string readTextFileQuiet(const char* path)
{
    return readTextFileImpl(path, false);
}

static GLuint compileShader(GLenum type, const char* src, const char* debugName)
{
    MIMITA_GL_CLEAR_STAGE("compileShader");
    GLuint shader = glCreateShader(type);
    MIMITA_GL_CHECK("glCreateShader");
    if (!shader)
        return 0;
    MIMITA_GL_CALL(glShaderSource(shader, 1, &src, nullptr));
    MIMITA_GL_CALL(glCompileShader(shader));

    GLint ok = 0;
    MIMITA_GL_CALL(glGetShaderiv(shader, GL_COMPILE_STATUS, &ok));

    if (!ok) {
        char log[2048];
        MIMITA_GL_CALL(glGetShaderInfoLog(shader, sizeof(log), nullptr, log));
        printf("[RENDERER] Shader compile failed: %s\n%s\n", debugName, log);
    } else {
        printf("[RENDERER] Shader compile OK: %s\n", debugName);
    }

    return shader;
}

static GLuint createProgramFromFiles(const char* vertPath, const char* fragPath)
{
    printf("[RENDERER] loading shaders\n");
    std::string vertText = readTextFile(vertPath);
    std::string fragText = readTextFile(fragPath);

    if (vertText.empty() || fragText.empty()) {
        printf("[RENDERER] Shader source missing\n");
        return 0;
    }

    GLuint vs = compileShader(GL_VERTEX_SHADER,   vertText.c_str(), vertPath);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragText.c_str(), fragPath);
    if (!vs || !fs)
    {
        if (vs) MIMITA_GL_CALL(glDeleteShader(vs));
        if (fs) MIMITA_GL_CALL(glDeleteShader(fs));
        return 0;
    }

    MIMITA_GL_CLEAR_STAGE("createProgramFromFiles");
    GLuint program = glCreateProgram();
    MIMITA_GL_CHECK("glCreateProgram");
    if (!program)
    {
        MIMITA_GL_CALL(glDeleteShader(vs));
        MIMITA_GL_CALL(glDeleteShader(fs));
        return 0;
    }
    MIMITA_GL_CALL(glAttachShader(program, vs));
    MIMITA_GL_CALL(glAttachShader(program, fs));
    MIMITA_GL_CALL(glLinkProgram(program));

    GLint ok = 0;
    MIMITA_GL_CALL(glGetProgramiv(program, GL_LINK_STATUS, &ok));

    if (!ok) {
        char log[2048];
        MIMITA_GL_CALL(glGetProgramInfoLog(program, sizeof(log), nullptr, log));
        printf("[RENDERER] Program link failed:\n%s\n", log);
    } else {
        printf("[RENDERER] Program link OK\n");
    }

    MIMITA_GL_CALL(glDeleteShader(vs));
    MIMITA_GL_CALL(glDeleteShader(fs));

    return program;
}

// Strict variant: returns 0 (and does not leak) on compile OR link failure.
static GLuint createProgramStrict(const std::string& vertText,
                                  const std::string& fragText,
                                  std::string* error)
{
    if (vertText.empty() || fragText.empty()) {
        if (error) *error = "shader source missing";
        return 0;
    }
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertText.c_str(), "live.vert");
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragText.c_str(), "live.frag");
    if (!vs || !fs) {
        if (vs) MIMITA_GL_CALL(glDeleteShader(vs));
        if (fs) MIMITA_GL_CALL(glDeleteShader(fs));
        if (error) *error = "compile failed";
        return 0;
    }
    GLuint program = glCreateProgram();
    if (!program) {
        MIMITA_GL_CALL(glDeleteShader(vs));
        MIMITA_GL_CALL(glDeleteShader(fs));
        if (error) *error = "program create failed";
        return 0;
    }
    MIMITA_GL_CALL(glAttachShader(program, vs));
    MIMITA_GL_CALL(glAttachShader(program, fs));
    MIMITA_GL_CALL(glLinkProgram(program));
    GLint ok = 0;
    MIMITA_GL_CALL(glGetProgramiv(program, GL_LINK_STATUS, &ok));
    MIMITA_GL_CALL(glDeleteShader(vs));
    MIMITA_GL_CALL(glDeleteShader(fs));
    if (!ok) {
        char log[2048];
        MIMITA_GL_CALL(glGetProgramInfoLog(program, sizeof(log), nullptr, log));
        printf("[RENDERER] live shader link failed:\n%s\n", log);
        MIMITA_GL_CALL(glDeleteProgram(program));
        if (error) *error = log;
        return 0;
    }
    return program;
}

static bool dimensionsAreValidForCursor(int width, int height)
{
    if (width <= 0 || height <= 0)
        return false;

    if (width > 256 || height > 256)
        return false;

    if (width > std::numeric_limits<int>::max() / height / 4)
        return false;

    return true;
}

Renderer::Renderer(int w, int h, const char* title) {
    width = w;
    height = h;

    if (!glfwInit()) {
        printf("GLFW init failed\n");
        return;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(w, h, title, nullptr, nullptr);
    if (!window) {
        printf("Window creation failed\n");
        glfwTerminate();
        return;
    }

    glfwSetCursorPosCallback(window,
    [](GLFWwindow* win, double x, double y)
    {
        Camera* cam = reinterpret_cast<Camera*>(glfwGetWindowUserPointer(win));
        if (!cam) return;
        if (glfwGetInputMode(win, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) {
            cam->firstMouse = true;
            return;
        }
        cam->updateMouse(x, y);
    });

    if (CursorConfig::CUSTOM_CURSOR_ENABLED) {
        installCustomCursor(CursorConfig::CUSTOM_CURSOR_PATH, CursorConfig::CUSTOM_CURSOR_HOTSPOT_CENTERED);
    }

    glfwMakeContextCurrent(window);

    // VSync is FORCED OFF and cannot be re-enabled.
    glfwSwapInterval(0);
    Debug::log(Debug::Category::Render,
        "[VSYNC] forced OFF: glfwSwapInterval(0) after context creation\n");

    // this might go here idk ? mar 6 2026
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // this just to test
    printf("[WINDOW] focused=%d\n", glfwGetWindowAttrib(window, GLFW_FOCUSED));

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        printf("GLAD init failed\n");
        return;
    }

    printf("OpenGL %s\n", glGetString(GL_VERSION));
    MIMITA_GL_CLEAR_STAGE("Renderer::Renderer after GLAD");
    MIMITA_GL_CALL(glEnable(GL_DEPTH_TEST));

    // do this mar 14 2026 dont do direct paths 
    shaderProgram = createProgramFromFiles(
        "shaders/basic.vert",
        "shaders/basic.frag"
    );

    // absolute paths idk why mar 6 2026 testing fix
    // dont do this it sucks mar 14 2026
    // shaderProgram = createProgramFromFiles(
    //     "C:/important/quiet/n/mimita-priv-v7/shaders/basic.vert",
    //     "C:/important/quiet/n/mimita-priv-v7/shaders/basic.frag"
    // );

    printf("[RENDERER] shaderProgram=%u\n", shaderProgram);

    // Register the basic shader as a generation-aware presentation resource so
    // edits to shaders/basic.* can hot-swap at runtime with last-good fallback.
    registerBasicShaderResource(this, shaderProgram);
}

namespace {

const std::uint64_t kBasicShaderLogical = gameHash("shader.basic");

void appendHash(std::uint64_t& hash, const std::string& text)
{
    for (unsigned char c : text) {
        hash ^= static_cast<std::uint64_t>(c);
        hash *= 1099511628211ull;
    }
}

struct ShaderFileStamp {
    std::uint64_t mtime = 0;
    std::uint64_t size = 0;
};

bool shaderFileStamp(const char* path, ShaderFileStamp& out)
{
    std::error_code ec;
    const std::string resolved = resolveAssetPath(path);
    const std::filesystem::file_time_type mtime =
        std::filesystem::last_write_time(resolved, ec);
    if (ec)
        return false;
    const std::uint64_t size = std::filesystem::file_size(resolved, ec);
    if (ec)
        return false;
    out.mtime = static_cast<std::uint64_t>(mtime.time_since_epoch().count());
    out.size = size;
    return true;
}

std::uint64_t basicShaderContentHash()
{
    // Only read and hash the two shader files when their size or mtime changed.
    // Live reload is preserved, but the per-frame blocking disk read is removed.
    static ShaderFileStamp sVertStamp;
    static ShaderFileStamp sFragStamp;
    static std::uint64_t sCachedHash = 0;
    static bool sHasCache = false;

    ShaderFileStamp vertNow;
    ShaderFileStamp fragNow;
    if (!shaderFileStamp("shaders/basic.vert", vertNow) ||
        !shaderFileStamp("shaders/basic.frag", fragNow))
        return sHasCache ? sCachedHash : 0;

    if (sHasCache &&
        vertNow.mtime == sVertStamp.mtime && vertNow.size == sVertStamp.size &&
        fragNow.mtime == sFragStamp.mtime && fragNow.size == sFragStamp.size)
        return sCachedHash;

    const std::string vert = readTextFileQuiet("shaders/basic.vert");
    const std::string frag = readTextFileQuiet("shaders/basic.frag");
    if (vert.empty() || frag.empty())
        return sHasCache ? sCachedHash : 0;
    std::uint64_t hash = 1469598103934665603ull;
    appendHash(hash, vert);
    appendHash(hash, frag);
    sVertStamp = vertNow;
    sFragStamp = fragNow;
    sCachedHash = hash;
    sHasCache = true;
    return hash;
}

bool loadBasicShaderResource(void* /*user*/, void** outHandle)
{
    const std::string vert = readTextFile("shaders/basic.vert");
    const std::string frag = readTextFile("shaders/basic.frag");
    std::string error;
    GLuint program = createProgramStrict(vert, frag, &error);
    if (!program)
        return false;
    *outHandle = reinterpret_cast<void*>(static_cast<std::uintptr_t>(program));
    return true;
}

void retireBasicShaderResource(void* /*user*/, void* handle)
{
    GLuint program = static_cast<GLuint>(reinterpret_cast<std::uintptr_t>(handle));
    if (program)
        MIMITA_GL_CALL(glDeleteProgram(program));
}

} // namespace

void Renderer::registerBasicShaderResource(Renderer* renderer, GLuint initialProgram)
{
    MimitaRuntime::PresentationResourceProvider& provider =
        MimitaRuntime::PresentationResourceProvider::instance();
    provider.setLoader(kBasicShaderLogical, &loadBasicShaderResource,
                       &retireBasicShaderResource, renderer);
    provider.adopt(kBasicShaderLogical, basicShaderContentHash(),
                   reinterpret_cast<void*>(static_cast<std::uintptr_t>(initialProgram)));
}

bool Renderer::pollShaderReload()
{
    const std::uint64_t hash = basicShaderContentHash();
    if (hash == 0)
        return false;
    MimitaRuntime::PresentationResourceProvider& provider =
        MimitaRuntime::PresentationResourceProvider::instance();
    const MimitaRuntime::ResourceGeneration* state =
        provider.current(kBasicShaderLogical);
    if (state && state->valid && state->contentHash == hash)
        return false;
    std::string error;
    if (!provider.apply(kBasicShaderLogical, hash, &error))
        return false;
    const MimitaRuntime::ResourceGeneration* after =
        provider.current(kBasicShaderLogical);
    if (after && after->valid && after->handle) {
        shaderProgram =
            static_cast<GLuint>(reinterpret_cast<std::uintptr_t>(after->handle));
        Debug::log(Debug::Category::Render,
                   "[SHADER] live reload generation=%u hash=%llu\n",
                   after->generation, (unsigned long long)hash);
        return true;
    }
    return false;
}

bool Renderer::installCustomCursor(const char* path, bool centeredHotspot)
{
    if (!window) {
        printf("[CURSOR] failed to install custom cursor: window is null\n");
        return false;
    }

    if (!path || !path[0]) {
        printf("[CURSOR] failed to load cursor.png: empty path\n");
        return false;
    }

    std::string resolvedPath = resolveAssetPath(path);

    int width = 0;
    int height = 0;
    int sourceChannels = 0;
    unsigned char* pixels = stbi_load(resolvedPath.c_str(), &width, &height, &sourceChannels, STBI_rgb_alpha);
    if (!pixels) {
        const char* reason = stbi_failure_reason();
        printf("[CURSOR] failed to load cursor.png path=%s reason=%s\n", resolvedPath.c_str(), reason ? reason : "unknown");
        return false;
    }

    if (!dimensionsAreValidForCursor(width, height)) {
        printf("[CURSOR] invalid cursor dimensions %dx%d path=%s\n", width, height, resolvedPath.c_str());
        stbi_image_free(pixels);
        return false;
    }

    GLFWimage image;
    image.width = width;
    image.height = height;
    image.pixels = pixels;

    int hotspotX = centeredHotspot ? width / 2 : 0;
    int hotspotY = centeredHotspot ? height / 2 : 0;
    if (hotspotX < 0 || hotspotX >= width || hotspotY < 0 || hotspotY >= height) {
        printf("[CURSOR] invalid hotspot %d,%d for cursor %dx%d path=%s\n",
               hotspotX, hotspotY, width, height, resolvedPath.c_str());
        stbi_image_free(pixels);
        return false;
    }

    const char* oldDescription = nullptr;
    glfwGetError(&oldDescription);
    GLFWcursor* nextCursor = glfwCreateCursor(&image, hotspotX, hotspotY);
    stbi_image_free(pixels);

    if (!nextCursor) {
        const char* description = nullptr;
        int errorCode = glfwGetError(&description);
        printf("[CURSOR] cursor creation failed %dx%d hotspot=%d,%d glfwError=%d %s\n",
               width,
               height,
               hotspotX,
               hotspotY,
               errorCode,
               description ? description : "");
        return false;
    }

    destroyCustomCursor();
    customCursor = nextCursor;
    glfwSetCursor(window, customCursor);

    printf("[CURSOR] loaded custom cursor %dx%d channels=%d hotspot=%d,%d mode=%s\n",
           width,
           height,
           sourceChannels,
           hotspotX,
           hotspotY,
           centeredHotspot ? "centered" : "top-left");
    return true;
}

void Renderer::destroyCustomCursor()
{
    if (!customCursor)
        return;

    if (window)
        glfwSetCursor(window, nullptr);

    glfwDestroyCursor(customCursor);
    customCursor = nullptr;
    printf("[CURSOR] destroyed custom cursor\n");
}

float Renderer::beginFrame() {
    static double last = glfwGetTime();
    double now = glfwGetTime();
    float dt = float(now - last);
    last = now;

    // Clamp the frame delta so a long presentation stall (e.g. ~1 s under
    // Wine) cannot inject a huge simulation step or a giant animation jump.
    // Mirrors FramePacer::beginFrame clamping (max 0.1 s).
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.1f) dt = 0.1f;

    glfwGetFramebufferSize(window, &width, &height);
    if (width <= 0) width = 1;
    if (height <= 0) height = 1;
    MIMITA_GL_CLEAR_STAGE("Renderer::beginFrame");
    MIMITA_GL_CALL(glViewport(0, 0, width, height));
    MIMITA_GL_CALL(glClearColor(0.1f, 0.1f, 0.12f, 1.0f));
    MIMITA_GL_CALL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    return dt;
}

void Renderer::endFrame() {
    glfwSwapBuffers(window);
    glfwPollEvents();
}

bool Renderer::shouldClose() {
    return window && glfwWindowShouldClose(window);
}

void Renderer::shutdown() {
    destroyCustomCursor();

    if (shaderProgram) {
        MIMITA_GL_CLEAR_STAGE("Renderer::shutdown");
        MIMITA_GL_CALL(glDeleteProgram(shaderProgram));
        shaderProgram = 0;
    }

    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }

    glfwTerminate();
}

void Renderer::applyVideoMode(int w, int h, bool fullscreen)
{
    if (!window) return;

    if (fullscreen) {
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        if (!monitor) {
            printf("[RENDERER] No primary monitor, falling back to windowed\n");
            fullscreen = false;
        } else {
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            glfwSetWindowMonitor(window, monitor, 0, 0, w, h,
                                 mode ? mode->refreshRate : GLFW_DONT_CARE);
        }
    }

    if (!fullscreen) {
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        int wx = 100, wy = 100;
        if (monitor) {
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            if (mode) {
                wx = (mode->width - w) / 2;
                wy = (mode->height - h) / 2;
            }
        }
        glfwSetWindowMonitor(window, nullptr, wx, wy, w, h, GLFW_DONT_CARE);
    }

    width = w;
    height = h;
    printf("[RENDERER] Video mode applied: %dx%d fullscreen=%d\n", w, h, (int)fullscreen);

    // Some GLFW/driver paths reset the swap interval when the window monitor
    // changes (fullscreen/windowed switch). Always force OFF.
    glfwSwapInterval(0);
    mVSync = false;
}

void Renderer::setVSync(bool on)
{
    if (on) {
        Debug::warn(Debug::Category::Render,
            "[VSYNC] BLOCKED attempt to enable VSync (swap interval forced to 0)\n");
    }
    if (window)
        glfwSwapInterval(0);
    mVSync = false;
    Debug::log(Debug::Category::Render,
        "[VSYNC] swap interval=0 (forced OFF)\n");
}

void Renderer::forceVSyncOff(const char* reason)
{
    mVSync = false;
    if (window)
        glfwSwapInterval(0);
    Debug::log(Debug::Category::Render,
        "[VSYNC] forced OFF: reason=%s\n", reason ? reason : "unknown");
}
