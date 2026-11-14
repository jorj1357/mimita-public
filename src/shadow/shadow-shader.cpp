// 2026-11-13
#include "shadow/shadow-shader.h"

namespace mimita::game::render {
namespace {

const char* kVertexSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 uLightMVP;
void main() {
    gl_Position = uLightMVP * vec4(aPos, 1.0);
}
)";

const char* kFragmentSource = R"(
#version 330 core
void main() {
}
)";

}  // namespace

bool ShadowShader::ensureLoaded() {
    if (mProgram.valid())
        return true;
    if (mProgram.load("shaders/shadow.vert", "shaders/shadow.frag"))
        return true;
    return mProgram.build(kVertexSource, kFragmentSource);
}

bool ShadowShader::reloadIfChanged() {
    if (!mProgram.valid())
        return ensureLoaded();
    return mProgram.reloadIfChanged();
}

ShadowShader& shadowShader() {
    static ShadowShader instance;
    return instance;
}

}  // namespace mimita::game::render
