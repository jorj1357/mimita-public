// 2026-11-13
/* purpose
* Hot, game-owned shader program.
* The game generation owns the program handle, the source files, the reload
* decision, and the uniform-location cache. The resident host only provides the
* primitive create/compile/link/location/delete operations.
* Editing the shader source files reloads live; editing this C++ is a normal
* hot DLL edit. Does NOT create or own the device.
*/
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "render/render-types.h"

namespace mimita::game::render {

class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram();

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    // Compile and link from source text. Keeps the previous program on failure.
    bool build(const char* vertexSource, const char* fragmentSource);

    // Read two files, then build. Records the files for reloadIfChanged().
    bool load(const char* vertexPath, const char* fragmentPath);

    // Rebuild when either source file changed. Keeps the previous program when
    // the candidate fails to compile or link.
    bool reloadIfChanged();

    bool valid() const { return mProgram != 0; }
    Program program() const { return mProgram; }
    void use() const;

    // Cached uniform location; -1 when absent. The cache is cleared on rebuild.
    Uniform uniform(const char* name);

    void destroy();

private:
    Program mProgram = 0;
    std::unordered_map<std::string, std::int32_t> mUniforms;
    std::string mVertexPath;
    std::string mFragmentPath;
    std::uint64_t mVertexWriteTime = 0;
    std::uint64_t mFragmentWriteTime = 0;
};

}  // namespace mimita::game::render
