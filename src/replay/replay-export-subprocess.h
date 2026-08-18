// 08 17 2026
/* purpose
* Runs a replay export as a standalone subprocess (separate mimita.exe process).
* Called after gameInit when --export-replay is specified on the command line.
* Owns the subprocess capture loop, map loading, and exit lifecycle.
* Does NOT own the main game loop, networking, or live gameplay.
*/
#pragma once

struct Engine;

// Run the export subprocess: load clip, load world, render frames, encode, outro, exit.
void runExportSubprocess(Engine& engine, const char* clipPath, const char* outputPath,
                         int width, int height);
