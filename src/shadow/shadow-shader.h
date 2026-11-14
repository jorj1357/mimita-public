// 2026-11-13
/* purpose
* Game-owned shadow-depth shader.
* The generation owns the program, the source files, the reload decision, and
* the uniform-location cache. The resident host only performs primitive shader
* operations; it does not own a shader registry or reload policy.
* Does NOT own the shadow framebuffer or the device.
*/
#pragma once

#include "render/shader-program.h"

namespace mimita::game::render {

class ShadowShader {
public:
    // Load shaders/shadow.vert|frag, falling back to the embedded source when
    // the files are missing. Keeps a valid program on failure.
    bool ensureLoaded();
    bool reloadIfChanged();

    bool valid() const { return mProgram.valid(); }
    Program program() const { return mProgram.program(); }
    ShaderProgram& programObject() { return mProgram; }
    Uniform uniform(const char* name) { return mProgram.uniform(name); }
    void use() const { mProgram.use(); }

private:
    ShaderProgram mProgram;
};

// One process-wide shadow shader owned by the game generation.
ShadowShader& shadowShader();

}  // namespace mimita::game::render
