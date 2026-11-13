// 2026-11-13
// Transitional EXE-side GL object lifetime for pre-cutover game code.

#include <glad/glad.h>

#include "game/gl-resource-bridge-module.h"

namespace mimita::game {
std::uint32_t genTexture() { GLuint id = 0; glGenTextures(1, &id); return id; }
void deleteTexture(std::uint32_t id) { GLuint v = id; glDeleteTextures(1, &v); }
std::uint32_t genBuffer() { GLuint id = 0; glGenBuffers(1, &id); return id; }
void deleteBuffer(std::uint32_t id) { GLuint v = id; glDeleteBuffers(1, &v); }
std::uint32_t genVertexArray() { GLuint id = 0; glGenVertexArrays(1, &id); return id; }
void deleteVertexArray(std::uint32_t id) { GLuint v = id; glDeleteVertexArrays(1, &v); }
std::uint32_t genFramebuffer() { GLuint id = 0; glGenFramebuffers(1, &id); return id; }
void deleteFramebuffer(std::uint32_t id) { GLuint v = id; glDeleteFramebuffers(1, &v); }
std::uint32_t genRenderbuffer() { GLuint id = 0; glGenRenderbuffers(1, &id); return id; }
void deleteRenderbuffer(std::uint32_t id) { GLuint v = id; glDeleteRenderbuffers(1, &v); }
std::uint32_t genQuery() { GLuint id = 0; glGenQueries(1, &id); return id; }
void deleteQuery(std::uint32_t id) { GLuint v = id; glDeleteQueries(1, &v); }
void deleteShader(std::uint32_t id) { glDeleteShader(id); }
void deleteProgram(std::uint32_t id) { glDeleteProgram(id); }
void generateMipmap(std::uint32_t target) { glGenerateMipmap(target); }
}  // namespace mimita::game
