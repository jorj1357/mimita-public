#pragma once

#include <cstdarg>
#include "../config.h"

namespace Debug
{
    enum class Category
    {
        General,
        GLB,
        Collision,
        Physics,
        Render
    };

    bool enabled(Category category);
    void startupReport();
    void log(Category category, const char* fmt, ...);
    void logOnce(Category category, const char* key, const char* fmt, ...);
    void logThrottled(Category category, const char* key, float intervalSeconds, const char* fmt, ...);
    void warn(Category category, const char* fmt, ...);
    void error(Category category, const char* fmt, ...);
    void logAuto(Category category, const char* fmt, ...);
}
