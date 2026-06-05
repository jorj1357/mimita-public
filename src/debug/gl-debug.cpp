#include "debug/gl-debug.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

bool traceVerbose()
{
    static int enabled = -1;
    if (enabled < 0)
        enabled = std::getenv("MIMITA_GL_TRACE_VERBOSE") ? 1 : 0;
    return enabled != 0;
}

const char* errorName(GLenum err)
{
    switch (err)
    {
        case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
        case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
        case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
        default: return "UNKNOWN";
    }
}

} // namespace

namespace GLDebug {

void clearErrors(const char* stage)
{
    GLenum err = glGetError();
    bool hadError = false;
    while (err != GL_NO_ERROR)
    {
        hadError = true;
        printf("[GL TRACE] cleared stale error before %s err=0x%X (%s)\n",
               stage ? stage : "unknown", err, errorName(err));
        err = glGetError();
    }

    if (traceVerbose() && !hadError)
        printf("[GL TRACE] clear before %s err=0x0\n", stage ? stage : "unknown");
}

bool logErrors(const char* label, const char* file, int line)
{
    GLenum err = glGetError();
    if (err == GL_NO_ERROR)
    {
        if (traceVerbose())
            printf("[GL TRACE] after %s err=0x0 (%s:%d)\n", label ? label : "unknown", file ? file : "?", line);
        return false;
    }

    bool hadError = false;
    while (err != GL_NO_ERROR)
    {
        hadError = true;
        printf("[GL TRACE] after %s err=0x%X (%s) at %s:%d\n",
               label ? label : "unknown", err, errorName(err), file ? file : "?", line);
        err = glGetError();
    }
    return hadError;
}

void traceBefore(const char* label, const char* file, int line)
{
    if (traceVerbose())
        printf("[GL TRACE] before %s (%s:%d)\n", label ? label : "unknown", file ? file : "?", line);
}

bool extensionSupported(const char* extensionName)
{
    if (!extensionName || !extensionName[0])
        return false;

#ifdef GLAD_GL_EXT_texture_filter_anisotropic
    if (std::strcmp(extensionName, "GL_EXT_texture_filter_anisotropic") == 0)
        return GLAD_GL_EXT_texture_filter_anisotropic != 0;
#endif

    GLint count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    if (logErrors("glGetIntegerv(GL_NUM_EXTENSIONS)", __FILE__, __LINE__))
        return false;

    for (GLint i = 0; i < count; ++i)
    {
        const char* ext = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, (GLuint)i));
        if (ext && std::strcmp(ext, extensionName) == 0)
            return true;
    }
    logErrors("glGetStringi(GL_EXTENSIONS)", __FILE__, __LINE__);
    return false;
}

} // namespace GLDebug
