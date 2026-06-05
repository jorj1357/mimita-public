#pragma once

#include <glad/glad.h>

namespace GLDebug {

void clearErrors(const char* stage);
bool logErrors(const char* label, const char* file, int line);
void traceBefore(const char* label, const char* file, int line);
bool extensionSupported(const char* extensionName);

} // namespace GLDebug

#define MIMITA_GL_CLEAR_STAGE(stage) ::GLDebug::clearErrors((stage))
#define MIMITA_GL_CHECK(label) ::GLDebug::logErrors((label), __FILE__, __LINE__)
#define MIMITA_GL_CALL(call) \
    do { \
        ::GLDebug::traceBefore(#call, __FILE__, __LINE__); \
        (call); \
        ::GLDebug::logErrors(#call, __FILE__, __LINE__); \
    } while (0)
