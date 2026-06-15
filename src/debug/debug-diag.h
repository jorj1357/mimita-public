#pragma once

#include <string>
#include <vector>
#include <cstdint>

#define GLFW_INCLUDE_NONE
#include <glad/glad.h>

// Central self-diagnostic system.
// Owns all engine introspection commands.
void registerDiagnosticCommands();

// Global structured log ring buffer
void diagLog(const char* category, const char* format, ...);
void diagLogGL(const char* file, int line, const char* op, GLenum error);

// GL error checking helper — wraps glGetError after operations
void diagCheckGL(const char* file, int line, const char* op);

#define CHECK_GL(op) do { op; diagCheckGL(__FILE__, __LINE__, #op); } while(0)

void diagRenderFrameBegin(float dt);
bool diagRenderTraceSampling();
void diagRenderStage(int stage);
void diagRenderWorldCounts(size_t batches, size_t vertices, size_t drawCalls);
void diagRenderCountPlayerDraw();
void diagRenderCountWeaponDraw();
void diagRenderCountEffectDraw();
void diagRenderFrameEnd();
