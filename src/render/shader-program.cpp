// 2026-11-13
// Game-owned ShaderProgram.
//
// The program handle, sources, reload detection, and uniform cache belong to the
// game generation. The render-command helpers resolve to raw GL in the resident
// EXE and to CAP_RENDER in the game DLL.

#include "render/shader-program.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "game/gl-resource-bridge-module.h"
#include "render/render-command.h"

namespace mimita::game::render {
namespace {

bool readTextFile(const std::string& path, std::string& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
        return false;
    out.assign(std::istreambuf_iterator<char>(file),
               std::istreambuf_iterator<char>());
    return !out.empty();
}

std::uint64_t writeTime(const std::string& path) {
    std::error_code error;
    const auto time = std::filesystem::last_write_time(path, error);
    if (error)
        return 0;
    return static_cast<std::uint64_t>(time.time_since_epoch().count());
}

void logInfoLog(const char* label, const char* log) {
    std::printf("[SHADER] %s: %s\n", label, log ? log : "(no log)");
}

}  // namespace

ShaderProgram::~ShaderProgram() {
    destroy();
}

bool ShaderProgram::build(const char* vertexSource, const char* fragmentSource) {
    if (!vertexSource || !fragmentSource)
        return false;

    Shader vertex = createShader(ShaderStage::Vertex);
    Shader fragment = createShader(ShaderStage::Fragment);
    if (!vertex || !fragment) {
        if (vertex) mimita::game::deleteShader(vertex);
        if (fragment) mimita::game::deleteShader(fragment);
        return false;
    }

    shaderSource(vertex, vertexSource);
    compileShader(vertex);
    shaderSource(fragment, fragmentSource);
    compileShader(fragment);

    std::int32_t ok = 0;
    getShaderiv(vertex, ShaderParameter::CompileStatus, &ok);
    if (!ok) {
        char log[1024] = {};
        getShaderInfoLog(vertex, static_cast<std::int32_t>(sizeof(log)), nullptr, log);
        logInfoLog("vertex compile error", log);
        mimita::game::deleteShader(vertex);
        mimita::game::deleteShader(fragment);
        return false;
    }

    getShaderiv(fragment, ShaderParameter::CompileStatus, &ok);
    if (!ok) {
        char log[1024] = {};
        getShaderInfoLog(fragment, static_cast<std::int32_t>(sizeof(log)), nullptr, log);
        logInfoLog("fragment compile error", log);
        mimita::game::deleteShader(vertex);
        mimita::game::deleteShader(fragment);
        return false;
    }

    Program program = createProgram();
    if (!program) {
        mimita::game::deleteShader(vertex);
        mimita::game::deleteShader(fragment);
        return false;
    }
    attachShader(program, vertex);
    attachShader(program, fragment);
    linkProgram(program);

    getProgramiv(program, ShaderParameter::LinkStatus, &ok);
    mimita::game::deleteShader(vertex);
    mimita::game::deleteShader(fragment);
    if (!ok) {
        char log[1024] = {};
        getProgramInfoLog(program, static_cast<std::int32_t>(sizeof(log)), nullptr, log);
        logInfoLog("link error", log);
        mimita::game::deleteProgram(program);
        return false;
    }

    if (mProgram)
        mimita::game::deleteProgram(mProgram);
    mProgram = program;
    mUniforms.clear();
    return true;
}

bool ShaderProgram::load(const char* vertexPath, const char* fragmentPath) {
    if (!vertexPath || !fragmentPath)
        return false;
    std::string vertexSource;
    std::string fragmentSource;
    if (!readTextFile(vertexPath, vertexSource) ||
        !readTextFile(fragmentPath, fragmentSource)) {
        std::printf("[SHADER] failed to read %s or %s\n", vertexPath, fragmentPath);
        return false;
    }
    if (!build(vertexSource.c_str(), fragmentSource.c_str()))
        return false;
    mVertexPath = vertexPath;
    mFragmentPath = fragmentPath;
    mVertexWriteTime = writeTime(mVertexPath);
    mFragmentWriteTime = writeTime(mFragmentPath);
    return true;
}

bool ShaderProgram::reloadIfChanged() {
    if (mVertexPath.empty() || mFragmentPath.empty())
        return false;
    const std::uint64_t vertexTime = writeTime(mVertexPath);
    const std::uint64_t fragmentTime = writeTime(mFragmentPath);
    if (vertexTime == 0 || fragmentTime == 0)
        return false;
    if (vertexTime == mVertexWriteTime && fragmentTime == mFragmentWriteTime)
        return false;

    std::string vertexSource;
    std::string fragmentSource;
    if (!readTextFile(mVertexPath, vertexSource) ||
        !readTextFile(mFragmentPath, fragmentSource))
        return false;
    if (!build(vertexSource.c_str(), fragmentSource.c_str()))
        return false;

    mVertexWriteTime = vertexTime;
    mFragmentWriteTime = fragmentTime;
    return true;
}

void ShaderProgram::use() const {
    if (mProgram)
        useProgram(mProgram);
}

Uniform ShaderProgram::uniform(const char* name) {
    if (!mProgram || !name)
        return -1;
    const auto found = mUniforms.find(name);
    if (found != mUniforms.end())
        return found->second;
    const Uniform location = getUniformLocation(mProgram, name);
    mUniforms.emplace(name, location);
    return location;
}

void ShaderProgram::destroy() {
    if (mProgram) {
        mimita::game::deleteProgram(mProgram);
        mProgram = 0;
    }
    mUniforms.clear();
}

}  // namespace mimita::game::render
