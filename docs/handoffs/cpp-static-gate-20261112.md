# Game static gate (2026-11-12)

**Status:** static scan of game-owned sources for raw lifetime patterns that
must use a resident capability instead. This is not a semantic review and not
runtime evidence.

- Game sources scanned: 598
- Violations: 232
- Files with violations: 61

## Rule counts

| Rule | Capability | Violations |
|---|---|---:|
| raw_thread | CAP_JOBS | 26 |
| raw_gl_resource | CAP_RESOURCES | 158 |
| raw_socket | CAP_TRANSPORT | 20 |
| raw_file_handle | CAP_RESOURCES | 28 |

## Files

| File | Rule | Line | Snippet |
|---|---|---:|---|
| `src/analytics/analytics-uploader.cpp` | raw_thread | 149 | `std::thread(std::move(send)).detach();` |
| `src/avatar/avatar-atlas.cpp` | raw_gl_resource | 48 | `glDeleteTextures(1, &atlasTexture);` |
| `src/avatar/avatar-atlas.cpp` | raw_gl_resource | 451 | `glDeleteTextures(1, &mAtlasTexture);` |
| `src/avatar/avatar-atlas.cpp` | raw_gl_resource | 455 | `glGenTextures(1, &mAtlasTexture);` |
| `src/avatar/avatar-atlas.cpp` | raw_gl_resource | 463 | `glGenerateMipmap(GL_TEXTURE_2D);` |
| `src/avatar/avatar-atlas.cpp` | raw_thread | 485 | `std::thread t([result, avatarCopy, basePathCopy]() {` |
| `src/avatar/avatar-atlas.cpp` | raw_thread | 489 | `t.detach();` |
| `src/avatar/avatar-atlas.cpp` | raw_gl_resource | 512 | `glDeleteTextures(1, &mAtlasTexture);` |
| `src/avatar/avatar-atlas.cpp` | raw_gl_resource | 516 | `glGenTextures(1, &mAtlasTexture);` |
| `src/avatar/avatar-atlas.cpp` | raw_gl_resource | 524 | `glGenerateMipmap(GL_TEXTURE_2D);` |
| `src/avatar/avatar-atlas.cpp` | raw_gl_resource | 860 | `glDeleteTextures(1, &inst.atlasTexture);` |
| `src/avatar/avatar-atlas.cpp` | raw_gl_resource | 864 | `glGenTextures(1, &inst.atlasTexture);` |
| `src/avatar/avatar-atlas.cpp` | raw_gl_resource | 872 | `glGenerateMipmap(GL_TEXTURE_2D);` |
| `src/avatar/cosmetic-system.cpp` | raw_gl_resource | 242 | `glGenVertexArrays(1, &cosmeticVAO);` |
| `src/avatar/cosmetic-system.cpp` | raw_gl_resource | 247 | `glGenBuffers(1, &cosmeticVBO);` |
| `src/combat/death-system.cpp` | raw_file_handle | 358 | `FILE* debugFile = fopen("logs/map_spawn_debug.txt", "a");` |
| `src/combat/projectile-render.cpp` | raw_gl_resource | 48 | `glGenVertexArrays(1, &cached.vao);` |
| `src/combat/projectile-render.cpp` | raw_gl_resource | 49 | `glGenBuffers(1, &cached.vbo);` |
| `src/combat/projectile-render.cpp` | raw_gl_resource | 332 | `glGenVertexArrays(1, &mesh.vao);` |
| `src/combat/projectile-render.cpp` | raw_gl_resource | 333 | `glGenBuffers(1, &mesh.vbo);` |
| `src/combat/projectile-render.cpp` | raw_gl_resource | 479 | `if (m.vbo) glDeleteBuffers(1, &m.vbo);` |
| `src/combat/projectile-render.cpp` | raw_gl_resource | 480 | `if (m.vao) glDeleteVertexArrays(1, &m.vao);` |
| `src/combat/revolver-system.cpp` | raw_gl_resource | 140 | `glGenVertexArrays(1, &mVao);` |
| `src/combat/revolver-system.cpp` | raw_gl_resource | 141 | `glGenBuffers(1, &mVbo);` |
| `src/combat/weapon-godball-movement.cpp` | raw_file_handle | 64 | `gGodballLogFile = fopen(path.c_str(), "w");` |
| `src/combat/weapon-model-cache.cpp` | raw_gl_resource | 41 | `glGenTextures(1, &tex);` |
| `src/combat/weapon-model-cache.cpp` | raw_gl_resource | 57 | `glGenerateMipmap(GL_TEXTURE_2D);` |
| `src/combat/weapon-model-cache.cpp` | raw_thread | 137 | `std::thread t(weaponModelLoadThread, asset);` |
| `src/combat/weapon-model-cache.cpp` | raw_thread | 138 | `t.detach();` |
| `src/combat/weapon-model-cache.cpp` | raw_gl_resource | 183 | `glGenVertexArrays(1, &asset->vao);` |
| `src/combat/weapon-model-cache.cpp` | raw_gl_resource | 184 | `glGenBuffers(1, &asset->vbo);` |
| `src/combat/weapon-runtime.cpp` | raw_file_handle | 145 | `gSpawnDebugFile = fopen("logs/spawn_weapon_debug.txt", "a");` |
| `src/combat/weapon-spyknife.cpp` | raw_file_handle | 76 | `gSpyknifeLogFile = fopen(path, "w");` |
| `src/debug/debug-diag-commands.cpp` | raw_file_handle | 242 | `FILE* f = fopen("diag-log.txt", "w");` |
| `src/debug/debug-visuals-lines.cpp` | raw_gl_resource | 83 | `MIMITA_GL_CALL(glGenVertexArrays(1, &gLineVao));` |
| `src/debug/debug-visuals-lines.cpp` | raw_gl_resource | 84 | `MIMITA_GL_CALL(glGenBuffers(1, &gLineVbo));` |
| `src/debug/debug-visuals-lines.cpp` | raw_gl_resource | 573 | `MIMITA_GL_CALL(glGenVertexArrays(1, &gWeaponLineVao));` |
| `src/debug/debug-visuals-lines.cpp` | raw_gl_resource | 574 | `MIMITA_GL_CALL(glGenBuffers(1, &gWeaponLineVbo));` |
| `src/debug/debug-visuals-tris.cpp` | raw_gl_resource | 49 | `MIMITA_GL_CALL(glGenVertexArrays(1, &vao));` |
| `src/debug/debug-visuals-tris.cpp` | raw_gl_resource | 50 | `MIMITA_GL_CALL(glGenBuffers(1, &vbo));` |
| `src/debug/npckillfeed-log.cpp` | raw_file_handle | 45 | `gLog = fopen(gPath.c_str(), "a");` |
| `src/devtools/scenario-recorder.cpp` | raw_thread | 1063 | `mMergeFuture = std::async(` |
| `src/effects/effect-part-render.cpp` | raw_gl_resource | 65 | `glGenVertexArrays(1, &cached.vao);` |
| `src/effects/effect-part-render.cpp` | raw_gl_resource | 66 | `glGenBuffers(1, &cached.vbo);` |
| `src/effects/effect-part-render.cpp` | raw_gl_resource | 139 | `glGenVertexArrays(1, &vao);` |
| `src/effects/effect-part-render.cpp` | raw_gl_resource | 140 | `glGenBuffers(1, &vbo);` |
| `src/engine/engine-tick-render.cpp` | raw_gl_resource | 376 | `glGenVertexArrays(1, &impactVao);` |
| `src/engine/engine-tick-render.cpp` | raw_gl_resource | 377 | `glGenBuffers(1, &impactVbo);` |
| `src/engine/engine-tick-render.cpp` | raw_gl_resource | 406 | `glGenVertexArrays(1, &vao);` |
| `src/engine/engine-tick-render.cpp` | raw_gl_resource | 407 | `glGenBuffers(1, &vbo);` |
| `src/engine/engine-tick-render.cpp` | raw_gl_resource | 973 | `glGenVertexArrays(1, &aftermathVao);` |
| `src/engine/engine-tick-render.cpp` | raw_gl_resource | 974 | `glGenBuffers(1, &aftermathVbo);` |
| `src/engine/engine-tick-replay.cpp` | raw_thread | 1005 | `std::thread([command, absolutePath]() {` |
| `src/engine/engine-tick-replay.cpp` | raw_thread | 1014 | `}).detach();` |
| `src/engine/engine-tick.cpp` | raw_file_handle | 172 | `FILE* roomFile = fopen(gRoomCodeFilePath.c_str(), "r");` |
| `src/entities/player-loader.cpp` | raw_gl_resource | 123 | `glGenTextures(1, &tex);` |
| `src/entities/player-loader.cpp` | raw_thread | 1356 | `std::thread t(modelLoadThreadFunc, raw);` |
| `src/entities/player-loader.cpp` | raw_thread | 1357 | `t.detach();` |
| `src/entities/player-loader.cpp` | raw_gl_resource | 1368 | `glGenTextures(1, &tex);` |
| `src/entities/player-loader.cpp` | raw_gl_resource | 1384 | `glGenerateMipmap(GL_TEXTURE_2D);` |
| `src/entities/player-render-mesh.cpp` | raw_gl_resource | 229 | `MIMITA_GL_CALL(glGenVertexArrays(1,&capsuleVAO));` |
| `src/entities/player-render-mesh.cpp` | raw_gl_resource | 230 | `MIMITA_GL_CALL(glGenBuffers(1,&capsuleVBO));` |
| `src/entities/player-render-mesh.cpp` | raw_gl_resource | 272 | `MIMITA_GL_CALL(glGenVertexArrays(1, &uploads.partVaos[partIndex]));` |
| `src/entities/player-render-mesh.cpp` | raw_gl_resource | 274 | `MIMITA_GL_CALL(glGenBuffers(1, &uploads.partVbos[partIndex]));` |
| `src/entities/player-render-mesh.cpp` | raw_gl_resource | 293 | `if (!bodyPartVAO) MIMITA_GL_CALL(glGenVertexArrays(1, &bodyPartVAO));` |
| `src/entities/player-render-mesh.cpp` | raw_gl_resource | 294 | `if (!bodyPartVBO) MIMITA_GL_CALL(glGenBuffers(1, &bodyPartVBO));` |
| `src/entities/player-render-mesh.cpp` | raw_gl_resource | 319 | `if (!uploads.renderVao) MIMITA_GL_CALL(glGenVertexArrays(1, &uploads.renderVao));` |
| `src/entities/player-render-mesh.cpp` | raw_gl_resource | 320 | `if (!uploads.renderVbo) MIMITA_GL_CALL(glGenBuffers(1, &uploads.renderVbo));` |
| `src/entities/player-render-mesh.cpp` | raw_gl_resource | 394 | `MIMITA_GL_CALL(glGenVertexArrays(1, &uploads.combinedVao));` |
| `src/entities/player-render-mesh.cpp` | raw_gl_resource | 396 | `MIMITA_GL_CALL(glGenBuffers(1, &uploads.combinedVbo));` |
| `src/game/spawn-utils.cpp` | raw_file_handle | 24 | `gSpawnDebugFile = fopen("logs/map_spawn_debug.txt", "a");` |
| `src/gui/font-stuff/font-loader.cpp` | raw_gl_resource | 94 | `MIMITA_GL_CALL(glGenTextures(1, &tex));` |
| `src/gui/font-stuff/font-loader.cpp` | raw_gl_resource | 139 | `MIMITA_GL_CALL(glGenVertexArrays(1,&guiVAO));` |
| `src/gui/font-stuff/font-loader.cpp` | raw_gl_resource | 140 | `MIMITA_GL_CALL(glGenBuffers(1,&guiVBO));` |
| `src/gui/font-stuff/font-loader.cpp` | raw_file_handle | 179 | `FILE* f = fopen(path, "rb");` |
| `src/gui/gui-main.cpp` | raw_socket | 314 | `SOCKET socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);` |
| `src/gui/gui-main.cpp` | raw_socket | 338 | `closesocket(socketHandle);` |
| `src/gui/gui-main.cpp` | raw_file_handle | 379 | `FILE* clearFile = fopen(roomFilePath.c_str(), "w");` |
| `src/gui/gui-main.cpp` | raw_file_handle | 730 | `FILE* f = fopen(gPendingServerRoomFilePath.c_str(), "r");` |
| `src/gui/gui-main.cpp` | raw_file_handle | 979 | `FILE* f = fopen(gPendingServerRoomFilePath.c_str(), "r");` |
| `src/gui/gui-media.cpp` | raw_gl_resource | 39 | `glGenTextures(1, &tex);` |
| `src/gui/gui-media.cpp` | raw_gl_resource | 79 | `if (t) glDeleteTextures(1, &t);` |
| `src/gui/gui-media.cpp` | raw_file_handle | 93 | `FILE* f = fopen(path, "rb");` |
| `src/gui/gui-media.cpp` | raw_gl_resource | 152 | `if (t) glDeleteTextures(1, &t);` |
| `src/gui/ui-system-render.cpp` | raw_gl_resource | 157 | `MIMITA_GL_CALL(glDeleteShader(vert));` |
| `src/gui/ui-system-render.cpp` | raw_gl_resource | 158 | `MIMITA_GL_CALL(glDeleteShader(frag));` |
| `src/gui/ui-system-render.cpp` | raw_gl_resource | 176 | `MIMITA_GL_CALL(glGenVertexArrays(1, &gVao));` |
| `src/gui/ui-system-render.cpp` | raw_gl_resource | 177 | `MIMITA_GL_CALL(glGenBuffers(1, &gVbo));` |
| `src/impact/destructible-async-cut.cpp` | raw_thread | 49 | `std::thread thread;` |
| `src/impact/destructible-async-cut.cpp` | raw_thread | 61 | `mImpl->thread = std::thread([this] {` |
| `src/impact/destructible-render.cpp` | raw_gl_resource | 55 | `if (pair.second.vbo) glDeleteBuffers(1, &pair.second.vbo);` |
| `src/impact/destructible-render.cpp` | raw_gl_resource | 56 | `if (pair.second.vao) glDeleteVertexArrays(1, &pair.second.vao);` |
| `src/impact/destructible-render.cpp` | raw_gl_resource | 70 | `if (it->second.vbo) glDeleteBuffers(1, &it->second.vbo);` |
| `src/impact/destructible-render.cpp` | raw_gl_resource | 71 | `if (it->second.vao) glDeleteVertexArrays(1, &it->second.vao);` |
| `src/impact/destructible-render.cpp` | raw_gl_resource | 124 | `glGenVertexArrays(1, &gpu.vao);` |
| `src/impact/destructible-render.cpp` | raw_gl_resource | 125 | `glGenBuffers(1, &gpu.vbo);` |
| `src/map/map-loader-material.cpp` | raw_gl_resource | 123 | `MIMITA_GL_CALL(glGenTextures(1, &tex));` |
| `src/map/map-loader-material.cpp` | raw_gl_resource | 161 | `MIMITA_GL_CALL(glGenerateMipmap(GL_TEXTURE_2D));` |
| `src/map/map-loader-material.cpp` | raw_gl_resource | 255 | `glGenTextures(1, &tex);` |
| `src/map/map_loader.cpp` | raw_gl_resource | 500 | `glDeleteTextures(` |
| `src/map/map_render.cpp` | raw_gl_resource | 18 | `//     glGenVertexArrays(1, &vao);` |
| `src/map/map_render.cpp` | raw_gl_resource | 19 | `//     glGenBuffers(1, &vbo);` |
| `src/map/texture.cpp` | raw_gl_resource | 38 | `MIMITA_GL_CALL(glGenTextures(1, &fallback));` |
| `src/map/texture.cpp` | raw_gl_resource | 64 | `MIMITA_GL_CALL(glGenTextures(1, &tex));` |
| `src/map/texture.cpp` | raw_gl_resource | 80 | `MIMITA_GL_CALL(glGenerateMipmap(GL_TEXTURE_2D));` |
| `src/network/client.cpp` | raw_socket | 77 | `SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);` |
| `src/network/client.cpp` | raw_socket | 95 | `closesocket(sock);` |
| `src/network/client.cpp` | raw_socket | 109 | `closesocket(sock);` |
| `src/network/client.cpp` | raw_socket | 118 | `closesocket(sock);` |
| `src/network/client.cpp` | raw_socket | 491 | `closesocket(sock);` |
| `src/network/multiplayer-packets.cpp` | raw_socket | 473 | `closesocket(ctx.sock);` |
| `src/network/multiplayer-packets.cpp` | raw_socket | 504 | `printf("[NET CONNECT] FATAL: WSAStartup failed\n");` |
| `src/network/multiplayer-packets.cpp` | raw_socket | 508 | `ctx.sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);` |
| `src/network/multiplayer-packets.cpp` | raw_socket | 511 | `printf("[NET CONNECT] FATAL: socket() failed error=%d\n", WSAGetLastError());` |
| `src/network/multiplayer-packets.cpp` | raw_socket | 537 | `closesocket(ctx.sock);` |
| `src/network/multiplayer-packets.cpp` | raw_thread | 999 | `std::thread thread;` |
| `src/network/multiplayer-packets.cpp` | raw_socket | 1071 | `printf("[ICE CONNECT] FATAL: WSAStartup failed\n");` |
| `src/network/multiplayer-packets.cpp` | raw_thread | 1475 | `gIceConnectJob->thread = std::thread(iceConnectWorker, roomCode, playerName);` |
| `src/network/server-browser.cpp` | raw_socket | 55 | `SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);` |
| `src/network/server-browser.cpp` | raw_socket | 78 | `closesocket(s);` |
| `src/network/server-browser.cpp` | raw_thread | 193 | `std::thread(refreshWorker).detach();` |
| `src/network/server-world.cpp` | raw_file_handle | 263 | `FILE* source = fopen(resolved.c_str(), "rb");` |
| `src/network/udp-echo.cpp` | raw_socket | 60 | `printf("[UDP ECHO FATAL] stage=startup error=WSAStartup\n");` |
| `src/network/udp-echo.cpp` | raw_socket | 64 | `SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);` |
| `src/network/udp-echo.cpp` | raw_socket | 83 | `closesocket(sock);` |
| `src/network/udp-echo.cpp` | raw_socket | 99 | `closesocket(sock);` |
| `src/network/udp-echo.cpp` | raw_socket | 172 | `closesocket(sock);` |
| `src/perf/perf-frame.cpp` | raw_thread | 83 | `std::thread gResourceWatchdogThread;` |
| `src/perf/perf-frame.cpp` | raw_thread | 188 | `gResourceWatchdogThread = std::thread(resourceWatchdogMain);` |
| `src/perf/perf-gpu.cpp` | raw_gl_resource | 50 | `glGenQueries(1, &region.begin);` |
| `src/perf/perf-gpu.cpp` | raw_gl_resource | 52 | `glGenQueries(1, &region.end);` |
| `src/perf/perf-visualize.cpp` | raw_file_handle | 22 | `FILE* f = fopen(path, "w");` |
| `src/physics/physical-entity.cpp` | raw_gl_resource | 1433 | `glGenVertexArrays(1, &gPhysicalBoxVao);` |
| `src/physics/physical-entity.cpp` | raw_gl_resource | 1434 | `glGenBuffers(1, &gPhysicalBoxVbo);` |
| `src/physics/physical-entity.cpp` | raw_gl_resource | 1527 | `glGenVertexArrays(1, &gPhysicalSphereVao);` |
| `src/physics/physical-entity.cpp` | raw_gl_resource | 1528 | `glGenBuffers(1, &gPhysicalSphereVbo);` |
| `src/physics/physical-entity.cpp` | raw_gl_resource | 1625 | `glGenVertexArrays(1, &cached.vao);` |
| `src/physics/physical-entity.cpp` | raw_gl_resource | 1626 | `glGenBuffers(1, &cached.vbo);` |
| `src/render/post-fx.cpp` | raw_gl_resource | 132 | `glGenVertexArrays(1, &vao);` |
| `src/render/post-fx.cpp` | raw_gl_resource | 133 | `glGenBuffers(1, &vbo);` |
| `src/render/post-fx.cpp` | raw_file_handle | 149 | `FILE* f = fopen(resolved.c_str(), "rb");` |
| `src/render/post-fx.cpp` | raw_gl_resource | 188 | `glDeleteShader(vs);` |
| `src/render/post-fx.cpp` | raw_gl_resource | 189 | `glDeleteShader(fs);` |
| `src/render/post-fx.cpp` | raw_gl_resource | 204 | `glDeleteProgram(prog);` |
| `src/render/post-fx.cpp` | raw_gl_resource | 208 | `glDeleteShader(vs);` |
| `src/render/post-fx.cpp` | raw_gl_resource | 209 | `glDeleteShader(fs);` |
| `src/render/post-fx.cpp` | raw_gl_resource | 247 | `glDeleteFramebuffers(1, &mFbo);` |
| `src/render/post-fx.cpp` | raw_gl_resource | 248 | `glDeleteTextures(1, &mColorTex);` |
| `src/render/post-fx.cpp` | raw_gl_resource | 254 | `glGenTextures(1, &mColorTex);` |
| `src/render/post-fx.cpp` | raw_gl_resource | 262 | `glGenFramebuffers(1, &mFbo);` |
| `src/render/post-fx.cpp` | raw_gl_resource | 267 | `glGenRenderbuffers(1, &depthRb);` |
| `src/render/post-fx.cpp` | raw_gl_resource | 276 | `glDeleteFramebuffers(1, &mFbo);` |
| `src/render/post-fx.cpp` | raw_gl_resource | 277 | `glDeleteTextures(1, &mColorTex);` |
| `src/render/post-fx.cpp` | raw_gl_resource | 278 | `glDeleteRenderbuffers(1, &depthRb);` |
| `src/render/procedural-cylinder.cpp` | raw_gl_resource | 146 | `glGenVertexArrays(1, &mesh.vao);` |
| `src/render/procedural-cylinder.cpp` | raw_gl_resource | 147 | `glGenBuffers(1, &mesh.vbo);` |
| `src/render/render-player.cpp` | raw_gl_resource | 1109 | `if (!vao) { glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo); }` |
| `src/render/render-world-mesh.cpp` | raw_gl_resource | 86 | `glGenVertexArrays(1, &gVao);` |
| `src/render/render-world-mesh.cpp` | raw_gl_resource | 87 | `glGenBuffers(1, &gVbo);` |
| `src/render/render-world-sky.cpp` | raw_gl_resource | 42 | `glGenVertexArrays(1, &gSkyVao);` |
| `src/render/render-world-sky.cpp` | raw_gl_resource | 43 | `glGenBuffers(1, &gSkyVbo);` |
| `src/render/skybox.cpp` | raw_file_handle | 59 | `FILE* f = fopen(path.c_str(), "rb");` |
| `src/render/skybox.cpp` | raw_gl_resource | 81 | `glDeleteShader(shader);` |
| `src/render/skybox.cpp` | raw_gl_resource | 91 | `glDeleteShader(vs); glDeleteShader(fs);` |
| `src/render/skybox.cpp` | raw_gl_resource | 104 | `glDeleteProgram(prog);` |
| `src/render/skybox.cpp` | raw_gl_resource | 107 | `glDeleteShader(vs);` |
| `src/render/skybox.cpp` | raw_gl_resource | 108 | `glDeleteShader(fs);` |
| `src/render/skybox.cpp` | raw_gl_resource | 130 | `if (mCubemapTex) { glDeleteTextures(1, &mCubemapTex); mCubemapTex = 0; }` |
| `src/render/skybox.cpp` | raw_gl_resource | 134 | `if (mVAO) { glDeleteVertexArrays(1, &mVAO); mVAO = 0; }` |
| `src/render/skybox.cpp` | raw_gl_resource | 135 | `if (mVBO) { glDeleteBuffers(1, &mVBO); mVBO = 0; }` |
| `src/render/skybox.cpp` | raw_gl_resource | 139 | `glGenVertexArrays(1, &mVAO);` |
| `src/render/skybox.cpp` | raw_gl_resource | 140 | `glGenBuffers(1, &mVBO);` |
| `src/render/skybox.cpp` | raw_gl_resource | 150 | `if (mShader) { glDeleteProgram(mShader); mShader = 0; }` |
| `src/render/skybox.cpp` | raw_gl_resource | 261 | `glGenTextures(1, &tex);` |
| `src/render/skybox.cpp` | raw_gl_resource | 288 | `glDeleteTextures(1, &tex);` |
| `src/replay/replay-commands-export.cpp` | raw_file_handle | 222 | `FILE* log = fopen(logPath.c_str(), "w");` |
| `src/replay/replay-export-ffmpeg.cpp` | raw_thread | 51 | `static std::thread gOutroThread;` |
| `src/replay/replay-export-ffmpeg.cpp` | raw_file_handle | 72 | `FILE* f = fopen(path.c_str(), "wb");` |
| `src/replay/replay-export-ffmpeg.cpp` | raw_file_handle | 621 | `FILE* bf = fopen(gJob.ffmpegBatchPath.c_str(), "w");` |
| `src/replay/replay-export-ffmpeg.cpp` | raw_thread | 797 | `gOutroThread = std::thread([output, outroW, outroH, outroAudio]() {` |
| `src/replay/replay-export-json.cpp` | raw_thread | 433 | `std::thread([cmd]() {` |
| `src/replay/replay-export-json.cpp` | raw_thread | 435 | `}).detach();` |
| `src/replay/replay-export-mf.cpp` | raw_file_handle | 164 | `FILE* f = fopen(path.c_str(), "rb");` |
| `src/replay/replay-export-mf.cpp` | raw_thread | 259 | `std::thread worker;` |
| `src/replay/replay-export-mf.cpp` | raw_thread | 1075 | `writer->worker = std::thread(workerLoop, writer);` |
| `src/replay/replay-export-subprocess.cpp` | raw_file_handle | 316 | `gJob.rawFile = fopen(gJob.rawTempPath.c_str(), "wb");` |
| `src/replay/replay-export-target.cpp` | raw_gl_resource | 23 | `glGenFramebuffers(1, &gTarget.fbo);` |
| `src/replay/replay-export-target.cpp` | raw_gl_resource | 26 | `glGenTextures(1, &gTarget.colorTex);` |
| `src/replay/replay-export-target.cpp` | raw_gl_resource | 37 | `glGenRenderbuffers(1, &gTarget.depthRbo);` |
| `src/replay/replay-export-target.cpp` | raw_gl_resource | 58 | `glDeleteFramebuffers(1, &gTarget.fbo);` |
| `src/replay/replay-export-target.cpp` | raw_gl_resource | 60 | `glDeleteTextures(1, &gTarget.colorTex);` |
| `src/replay/replay-export-target.cpp` | raw_gl_resource | 62 | `glDeleteRenderbuffers(1, &gTarget.depthRbo);` |
| `src/replay/replay-export.cpp` | raw_thread | 707 | `std::thread([args, path]() {` |
| `src/replay/replay-export.cpp` | raw_thread | 713 | `}).detach();` |
| `src/replay/replay-factory-clips.cpp` | raw_thread | 81 | `std::hash<std::thread::id>{}(std::this_thread::get_id()));` |
| `src/shadow/shadow-render.cpp` | raw_gl_resource | 79 | `glGenVertexArrays(1, &gSphereVao);` |
| `src/shadow/shadow-render.cpp` | raw_gl_resource | 80 | `glGenBuffers(1, &gSphereVbo);` |
| `src/shadow/shadow-render.cpp` | raw_gl_resource | 110 | `glDeleteShader(vs);` |
| `src/shadow/shadow-render.cpp` | raw_gl_resource | 111 | `glDeleteShader(fs);` |
| `src/shadow/shadow-render.cpp` | raw_gl_resource | 125 | `glDeleteProgram(prog);` |
| `src/shadow/shadow-render.cpp` | raw_gl_resource | 128 | `glDeleteShader(vs);` |
| `src/shadow/shadow-render.cpp` | raw_gl_resource | 129 | `glDeleteShader(fs);` |
| `src/shadow/shadow-render.cpp` | raw_file_handle | 136 | `FILE* f = fopen(path, "rb");` |
| `src/shadow/shadow-render.cpp` | raw_gl_resource | 184 | `glDeleteFramebuffers(1, &gShadowFbo);` |
| `src/shadow/shadow-render.cpp` | raw_gl_resource | 185 | `glDeleteTextures(1, &gShadowDepthTex);` |
| `src/shadow/shadow-render.cpp` | raw_gl_resource | 190 | `glGenTextures(1, &gShadowDepthTex);` |
| `src/shadow/shadow-render.cpp` | raw_gl_resource | 200 | `glGenFramebuffers(1, &gShadowFbo);` |
| `src/shadow/shadow-render.cpp` | raw_gl_resource | 209 | `glDeleteFramebuffers(1, &gShadowFbo);` |
| `src/shadow/shadow-render.cpp` | raw_gl_resource | 210 | `glDeleteTextures(1, &gShadowDepthTex);` |
| `src/shadow/shadow-render.cpp` | raw_gl_resource | 434 | `glGenVertexArrays(1, &vao);` |
| `src/shadow/shadow-render.cpp` | raw_gl_resource | 435 | `glGenBuffers(1, &vbo);` |
| `src/terminal/weapon-bench-commands.cpp` | raw_file_handle | 115 | `FILE* f = fopen("logs/latest-weapon-bench-path.txt", "w");` |
| `src/utils/pak-file.cpp` | raw_file_handle | 69 | `FILE* f = fopen(pakPath.c_str(), "rb");` |
| `src/utils/pak-file.cpp` | raw_file_handle | 156 | `FILE* out = fopen(destPath.c_str(), "wb");` |
| `src/utils/pak-file.cpp` | raw_file_handle | 178 | `FILE* out = fopen(destPath.c_str(), "wb");` |
| `src/world/streamed-world-layer.cpp` | raw_gl_resource | 307 | `if (record.vao) glDeleteVertexArrays(1, &record.vao);` |
| `src/world/streamed-world-layer.cpp` | raw_gl_resource | 308 | `if (record.vbo) glDeleteBuffers(1, &record.vbo);` |
| `src/world/streamed-world-layer.cpp` | raw_gl_resource | 327 | `if (templateData.vao) glDeleteVertexArrays(1, &templateData.vao);` |
| `src/world/streamed-world-layer.cpp` | raw_gl_resource | 328 | `if (templateData.vbo) glDeleteBuffers(1, &templateData.vbo);` |
| `src/world/streamed-world-layer.cpp` | raw_gl_resource | 329 | `if (templateData.instanceVbo) glDeleteBuffers(1, &templateData.instanceVbo);` |
| `src/world/streamed-world-layer.cpp` | raw_gl_resource | 689 | `if (templateData.vao == 0) glGenVertexArrays(1, &templateData.vao);` |
| `src/world/streamed-world-layer.cpp` | raw_gl_resource | 690 | `if (templateData.vbo == 0) glGenBuffers(1, &templateData.vbo);` |
| `src/world/streamed-world-layer.cpp` | raw_gl_resource | 708 | `if (templateData.instanceVbo == 0) glGenBuffers(1, &templateData.instanceVbo);` |
| `src/world/streamed-world-layer.cpp` | raw_gl_resource | 722 | `if (templateData.instanceVbo == 0) glGenBuffers(1, &templateData.instanceVbo);` |
| `src/world/streamed-world-layer.cpp` | raw_gl_resource | 743 | `if (record.vbo == 0) glGenBuffers(1, &record.vbo);` |
| `src/world/streamed-world-layer.cpp` | raw_gl_resource | 772 | `if (record.vao) glDeleteVertexArrays(1, &record.vao);` |
| `src/world/streamed-world-layer.cpp` | raw_gl_resource | 773 | `if (record.vbo) glDeleteBuffers(1, &record.vbo);` |
| `src/world/streamed-world-layer.cpp` | raw_gl_resource | 811 | `glGenVertexArrays(1, &vao);` |
| `src/world/streamed-world-layer.cpp` | raw_gl_resource | 1009 | `if (record.vao) glDeleteVertexArrays(1, &record.vao);` |
| `src/world/streamed-world-layer.cpp` | raw_gl_resource | 1010 | `if (record.vbo) glDeleteBuffers(1, &record.vbo);` |
| `src/world/texture-store.cpp` | raw_gl_resource | 61 | `glDeleteTextures(1, &it->second);` |
| `src/world/world-gltf-loader.cpp` | raw_file_handle | 57 | `FILE* f = fopen(resolvedPath.c_str(), "rb");` |
