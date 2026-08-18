// 08 17 2026
/* purpose
* Owns the offscreen framebuffer used to capture replay export frames without
* reading from the visible game window. Holds a color texture + depth buffer at
* the export capture resolution.
* Does NOT own replay export job state, encode backends, or the main render loop.
*/
#pragma once

struct ReplayExportTarget {
    unsigned int fbo = 0;
    unsigned int colorTex = 0;
    unsigned int depthRbo = 0;
    int width = 0;
    int height = 0;
    bool ready() const { return fbo != 0 && width > 0 && height > 0; }
};

bool replayExportTargetInit(int w, int h);
void replayExportTargetDestroy();
ReplayExportTarget& replayExportTarget();
