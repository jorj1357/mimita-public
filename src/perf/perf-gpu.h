// 08 31 2026, 11 40
/* purpose
* Deferred OpenGL timer-query measurements for deep performance sessions.
* Keeps GPU timing separate from CPU scope timing and avoids blocking waits.
* Reports completed GPU regions one or more frames after submission.
* DOES NOT call glFinish, synchronize gameplay, or affect rendering behavior.
* Does not collect timings when deep profiling is disabled.
* Does not own CPU scope aggregation or performance log formatting.
*/
#pragma once

namespace PerfGpu {
void beginFrame();
void beginRegion(const char* name);
void endRegion();
void endFrame();
void flushCompleted();
bool enabled();
void setEnabled(bool enabled);
double lastCompletedFrameMs();
double lastCompletedRegionMs(const char* name);
}
