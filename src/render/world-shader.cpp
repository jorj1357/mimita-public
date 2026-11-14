// 2026-11-13
#include "render/world-shader.h"

namespace mimita::game::render {

bool WorldShader::ensureLoaded() {
    if (mProgram.valid())
        return true;
    return mProgram.load("shaders/basic.vert", "shaders/basic.frag");
}

bool WorldShader::reloadIfChanged() {
    if (!mProgram.valid())
        return ensureLoaded();
    return mProgram.reloadIfChanged();
}

WorldShader& worldShader() {
    static WorldShader instance;
    return instance;
}

}  // namespace mimita::game::render
