#pragma once

enum DebugCategory {
    DC_COLLISION,
    DC_CONTACT,
    DC_RESOLVE,
    DC_PLAYER,
    DC_GRID,
    DC_COUNT
};

extern bool gDebugEnabled;

void debugLog(DebugCategory cat, const char* fmt, ...);
