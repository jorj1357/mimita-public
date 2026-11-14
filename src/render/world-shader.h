// 2026-11-13
/* purpose
* Game-owned main world shader.
* The generation owns the program, the source files, the reload decision, and
* the uniform-location cache. The resident host only performs primitive shader
* operations; it does not own a shader registry or reload policy.
* Does NOT create the device or change VSync.
*/
#pragma once

#include "render/shader-program.h"

namespace mimita::game::render {

class WorldShader {
public:
    // Load shaders/basic.vert|frag once. Returns false when the files are
    // missing or the candidate fails; a previously valid program is kept.
    bool ensureLoaded();

    // Recompile when either source file changed. A failed candidate keeps the
    // previous program active.
    bool reloadIfChanged();

    bool valid() const { return mProgram.valid(); }
    Program program() const { return mProgram.program(); }
    // The owning ShaderProgram, for renderers that take a ShaderProgram&.
    ShaderProgram& programObject() { return mProgram; }
    Uniform uniform(const char* name) { return mProgram.uniform(name); }
    void use() const { mProgram.use(); }

private:
    ShaderProgram mProgram;
};

// One process-wide world shader owned by the game generation.
WorldShader& worldShader();

}  // namespace mimita::game::render
