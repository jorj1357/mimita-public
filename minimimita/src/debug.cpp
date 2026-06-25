#include "debug.h"
#include <cstdio>
#include <cstdarg>
#include <cmath>

static double gLastLogTime[DC_COUNT] = {0.0, 0.0, 0.0, 0.0, 0.0};
static double gTimeSource = 0.0;
static const double RATE_LIMIT = 0.5;

bool gDebugEnabled = true;

static const char* categoryName(DebugCategory cat) {
    switch (cat) {
        case DC_COLLISION: return "COLLISION";
        case DC_CONTACT:    return "CONTACT";
        case DC_RESOLVE:    return "RESOLVE";
        case DC_PLAYER:     return "PLAYER";
        case DC_GRID:       return "GRID";
        default:            return "?";
    }
}

void debugLog(DebugCategory cat, const char* fmt, ...) {
    if (!gDebugEnabled) return;

    double now = gTimeSource;
    if (now - gLastLogTime[cat] < RATE_LIMIT) return;
    gLastLogTime[cat] = now;

    printf("[%s] ", categoryName(cat));
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

void debugLogTick(double time) {
    gTimeSource = time;
}
