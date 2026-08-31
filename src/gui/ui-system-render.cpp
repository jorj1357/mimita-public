#include "gui/ui-system.h"
#include "gui/ui-system-internal.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>

#include <glad/glad.h>
#include "gui/gui-media.h"
#include "gui/gui-coord.h"
#include "world/texture-store.h"
#include "audio/audio.h"
#include "gui/font-stuff/font-loader.h"
#include "debug/gl-debug.h"

extern TextureStore gTextures;

using namespace UISys;

namespace {

const unsigned char FONT5X7[44][7] = {
    {0,0,0,0,0,0,0},       // space
    {14,17,19,21,25,17,14},// 0
    {4,12,4,4,4,4,14},     // 1
    {14,17,1,2,4,8,31},    // 2
    {30,1,1,14,1,1,30},    // 3
    {2,6,10,18,31,2,2},    // 4
    {31,16,30,1,1,17,14},  // 5
    {6,8,16,30,17,17,14},  // 6
    {31,1,2,4,8,8,8},      // 7
    {14,17,17,14,17,17,14},// 8
    {14,17,17,15,1,2,12},  // 9
    {14,17,17,31,17,17,17},// A
    {30,17,17,30,17,17,30},// B
    {14,17,16,16,16,17,14},// C
    {30,17,17,17,17,17,30},// D
    {31,16,16,30,16,16,31},// E
    {31,16,16,30,16,16,16},// F
    {14,17,16,23,17,17,15},// G
    {17,17,17,31,17,17,17},// H
    {14,4,4,4,4,4,14},     // I
    {7,2,2,2,2,18,12},     // J
    {17,18,20,24,20,18,17},// K
    {16,16,16,16,16,16,31},// L
    {17,27,21,21,17,17,17},// M
    {17,25,21,19,17,17,17},// N
    {14,17,17,17,17,17,14},// O
    {30,17,17,30,16,16,16},// P
    {14,17,17,17,21,18,13},// Q
    {30,17,17,30,20,18,17},// R
    {15,16,16,14,1,1,30},  // S
    {31,4,4,4,4,4,4},      // T
    {17,17,17,17,17,17,14},// U
    {17,17,17,17,17,10,4}, // V
    {17,17,17,21,21,21,10},// W
    {17,17,10,4,10,17,17}, // X
    {17,17,10,4,4,4,4},    // Y
    {31,1,2,4,8,16,31},    // Z
    {0,0,0,14,0,0,0},      // -
    {0,0,0,0,0,12,12},     // .
    {0,0,0,0,0,0,4},       // ,
    {4,4,4,4,4,0,4},       // !
    {10,10,0,0,0,0,0},     // "
    {0,4,8,31,8,4,0},      // >
    {0,4,2,31,2,4,0},      // <
};

int glyphIndex(char c)
{
    if (c == ' ') return 0;
    if (c >= '0' && c <= '9') return 1 + (c - '0');
    if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') return 11 + (c - 'A');
    if (c == '-') return 37;
    if (c == '.') return 38;
    if (c == ',') return 39;
    if (c == '!') return 40;
    if (c == '"') return 41;
    if (c == '>') return 42;
    if (c == '<') return 43;
    return 37;
}

GLuint compile(GLenum type, const char* src, const char* name)
{
    GLuint s = glCreateShader(type);
    MIMITA_GL_CHECK("ui glCreateShader");
    if (!s)
        return 0;
    MIMITA_GL_CALL(glShaderSource(s, 1, &src, nullptr));
    MIMITA_GL_CALL(glCompileShader(s));
    GLint ok = 0;
    MIMITA_GL_CALL(glGetShaderiv(s, GL_COMPILE_STATUS, &ok));
    if (!ok) {
        char log[2048];
        MIMITA_GL_CALL(glGetShaderInfoLog(s, sizeof(log), nullptr, log));
        printf("[UI ERROR] shader compile failed: %s\n%s\n", name, log);
    }
    return s;
}

} // anonymous namespace

void ensureProgram()
{
    if (gProgram) return;

    printf("[UI] Initializing dedicated screen-space UI shader\n");
    const char* vs =
        "#version 330 core\n"
        "layout(location=0) in vec2 aPos;\n"
        "layout(location=1) in vec2 aUV;\n"
        "layout(location=2) in vec4 aColor;\n"
        "layout(location=3) in float aTextureMode;\n"
        "uniform vec2 uScreen;\n"
        "out vec2 vUV;\n"
        "out vec4 vColor;\n"
        "flat out int vTextureMode;\n"
        "void main(){\n"
        "  vec2 ndc = vec2((aPos.x/uScreen.x)*2.0-1.0, 1.0-(aPos.y/uScreen.y)*2.0);\n"
        "  vUV = aUV;\n"
        "  vColor = aColor;\n"
        "  vTextureMode = int(aTextureMode + 0.5);\n"
        "  gl_Position = vec4(ndc, 0.0, 1.0);\n"
        "}\n";
    const char* fs =
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "in vec2 vUV;\n"
        "in vec4 vColor;\n"
        "flat in int vTextureMode;\n"
        "uniform sampler2D uFontPage0;\n"
        "uniform sampler2D uFontPage1;\n"
        "uniform sampler2D uImageTex;\n"
        "void main(){\n"
        "  if (vTextureMode == 3) { FragColor = texture(uImageTex, vUV) * vColor; return; }\n"
        "  if (vTextureMode == 2) { FragColor = texture(uFontPage1, vUV) * vColor; return; }\n"
        "  if (vTextureMode == 1) { FragColor = texture(uFontPage0, vUV) * vColor; return; }\n"
        "  FragColor = vColor;\n"
        "}\n";

    MIMITA_GL_CLEAR_STAGE("ui ensureProgram");
    GLuint vert = compile(GL_VERTEX_SHADER, vs, "ui.vert");
    GLuint frag = compile(GL_FRAGMENT_SHADER, fs, "ui.frag");
    if (!vert || !frag)
        return;
    gProgram = glCreateProgram();
    MIMITA_GL_CHECK("ui glCreateProgram");
    if (!gProgram)
        return;
    MIMITA_GL_CALL(glAttachShader(gProgram, vert));
    MIMITA_GL_CALL(glAttachShader(gProgram, frag));
    MIMITA_GL_CALL(glLinkProgram(gProgram));
    MIMITA_GL_CALL(glDeleteShader(vert));
    MIMITA_GL_CALL(glDeleteShader(frag));

    GLint ok = 0;
    MIMITA_GL_CALL(glGetProgramiv(gProgram, GL_LINK_STATUS, &ok));
    if (!ok) {
        char log[2048];
        MIMITA_GL_CALL(glGetProgramInfoLog(gProgram, sizeof(log), nullptr, log));
        printf("[UI ERROR] program link failed\n%s\n", log);
    } else {
        printf("[UI] UI shader linked program=%u\n", gProgram);
    }

    gScreenLoc = glGetUniformLocation(gProgram, "uScreen");
    gColorLoc = -1;
    gUseTexLoc = -1;
    gTexLoc = -1;
    gImageTexLoc = glGetUniformLocation(gProgram, "uImageTex");

    MIMITA_GL_CALL(glGenVertexArrays(1, &gVao));
    MIMITA_GL_CALL(glGenBuffers(1, &gVbo));
    MIMITA_GL_CALL(glBindVertexArray(gVao));
    MIMITA_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, gVbo));
    MIMITA_GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(UIVertex) * 1024, nullptr, GL_DYNAMIC_DRAW));
    MIMITA_GL_CALL(glEnableVertexAttribArray(0));
    MIMITA_GL_CALL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)offsetof(UIVertex, position)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(1));
    MIMITA_GL_CALL(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)offsetof(UIVertex, uv)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(1));
    MIMITA_GL_CALL(glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)offsetof(UIVertex, color)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(2));
    MIMITA_GL_CALL(glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)offsetof(UIVertex, textureMode)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(3));
}

void drawTriVerts(const float* verts, int vertCount, glm::vec4 color, GLenum mode)
{
    if (!verts || vertCount <= 0 || mode != GL_TRIANGLES)
        return;
    for (int i = 0; i < vertCount; ++i)
        gBatchVertices.push_back({{verts[i * 4], verts[i * 4 + 1]}, {verts[i * 4 + 2], verts[i * 4 + 3]}, color, 0.0f});
}

void drawTexturedQuad(const float* verts, int vertCount, GLuint tex, glm::vec4 color)
{
    ensureProgram();
    if (!gProgram || !gVao || !gVbo || !verts || !tex || vertCount <= 0)
        return;
    uiFlushBatch();
    MIMITA_GL_CALL(glUseProgram(gProgram));
    MIMITA_GL_CALL(glEnable(GL_BLEND));
    MIMITA_GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    MIMITA_GL_CALL(glUniform2f(gScreenLoc, (float)gFbW, (float)gFbH));
    MIMITA_GL_CALL(glUniform1i(gImageTexLoc, 0));
    MIMITA_GL_CALL(glActiveTexture(GL_TEXTURE0));
    MIMITA_GL_CALL(glBindTexture(GL_TEXTURE_2D, tex));
    MIMITA_GL_CALL(glBindVertexArray(gVao));
    MIMITA_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, gVbo));
    static std::vector<UIVertex> imageVertices;
    imageVertices.clear();
    imageVertices.reserve(static_cast<size_t>(vertCount));
    for (int i = 0; i < vertCount; ++i)
        imageVertices.push_back({{verts[i * 4], verts[i * 4 + 1]}, {verts[i * 4 + 2], verts[i * 4 + 3]}, color, 3.0f});
    MIMITA_GL_CALL(glBufferData(GL_ARRAY_BUFFER, imageVertices.size() * sizeof(UIVertex), imageVertices.data(), GL_DYNAMIC_DRAW));
    MIMITA_GL_CALL(glDrawArrays(GL_TRIANGLES, 0, vertCount));
    ++gDrawCalls;
}

void uiFlushBatch()
{
    if (gBatchVertices.empty())
        return;
    ensureProgram();
    MIMITA_GL_CALL(glUseProgram(gProgram));
    MIMITA_GL_CALL(glUniform2f(gScreenLoc, (float)gFbW, (float)gFbH));
    static GLint fontPage0Loc = -1;
    static GLint fontPage1Loc = -1;
    if (fontPage0Loc < 0) {
        fontPage0Loc = glGetUniformLocation(gProgram, "uFontPage0");
        fontPage1Loc = glGetUniformLocation(gProgram, "uFontPage1");
    }
    MIMITA_GL_CALL(glUniform1i(fontPage0Loc, 0));
    MIMITA_GL_CALL(glUniform1i(fontPage1Loc, 1));
    MIMITA_GL_CALL(glActiveTexture(GL_TEXTURE0));
    MIMITA_GL_CALL(glBindTexture(GL_TEXTURE_2D, gFontPages[0]));
    MIMITA_GL_CALL(glActiveTexture(GL_TEXTURE1));
    MIMITA_GL_CALL(glBindTexture(GL_TEXTURE_2D, gFontPages[1]));
    MIMITA_GL_CALL(glBindVertexArray(gVao));
    MIMITA_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, gVbo));
    MIMITA_GL_CALL(glBufferData(GL_ARRAY_BUFFER, gBatchVertices.size() * sizeof(UIVertex), gBatchVertices.data(), GL_DYNAMIC_DRAW));
    MIMITA_GL_CALL(glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(gBatchVertices.size())));
    ++gDrawCalls;
    gBatchVertices.clear();
}

void debugWidget(const char* type, const char* name, UIRect r, bool hovered, bool pressed)
{
    if (!gDebug) return;

    uiDrawRectOutline(r, {1.0f, 0.0f, 1.0f, 1.0f}, "widget-bounds");
    uiDrawRect({r.x - 2.0f, r.y - 2.0f, 4.0f, 4.0f}, {1.0f, 0.2f, 0.2f, 1.0f}, "pivot");
    uiDrawRectOutline({r.x + 6.0f, r.y + 6.0f, r.w - 12.0f, r.h - 12.0f}, {0.0f, 0.9f, 1.0f, 1.0f}, "padding");
    char buf[256];
    snprintf(buf, sizeof(buf), "[%s: %s] hovered=%s pressed=%s bounds=(%.0f,%.0f,%.0f,%.0f)",
             type, name, hovered ? "true" : "false", pressed ? "true" : "false",
             r.x, r.y, r.w, r.h);
    uiDrawText(buf, r.x, r.y + r.h + 8.0f, 0.28f, {0.7f, 1.0f, 0.95f, 1.0f});
}

void uiDrawRect(UIRect r, glm::vec4 color, const char* debugName)
{
    if (r.w <= 0.0f || r.h <= 0.0f) {
        printf("[UI WARNING] bad rect %s %.1f %.1f %.1f %.1f\n", debugName ? debugName : "unnamed", r.x, r.y, r.w, r.h);
        return;
    }
    float verts[] = {
        r.x, r.y, 0, 0, r.x + r.w, r.y, 0, 0, r.x + r.w, r.y + r.h, 0, 0,
        r.x, r.y, 0, 0, r.x + r.w, r.y + r.h, 0, 0, r.x, r.y + r.h, 0, 0
    };
    drawTriVerts(verts, 6, color, GL_TRIANGLES);
}

void uiDrawTriangle(float cx, float cy, float size, bool pointDown, glm::vec4 color, const char* debugName)
{
    (void)debugName;
    float hs = size * 0.5f;
    float tipY = pointDown ? cy + hs : cy - hs;
    float baseY = pointDown ? cy - hs : cy + hs;
    float verts[] = {
        cx, tipY, 0, 0,
        cx - hs, baseY, 0, 0,
        cx + hs, baseY, 0, 0
    };
    drawTriVerts(verts, 3, color, GL_TRIANGLES);
}

void uiDrawRectOutline(UIRect r, glm::vec4 color, const char* debugName)
{
    (void)debugName;
    float verts[] = {
        r.x, r.y, 0, 0, r.x + r.w, r.y, 0, 0,
        r.x + r.w, r.y, 0, 0, r.x + r.w, r.y + r.h, 0, 0,
        r.x + r.w, r.y + r.h, 0, 0, r.x, r.y + r.h, 0, 0,
        r.x, r.y + r.h, 0, 0, r.x, r.y, 0, 0
    };
    uiDrawRect({r.x, r.y, r.w, 1.0f}, color, debugName);
    uiDrawRect({r.x, r.y + r.h - 1.0f, r.w, 1.0f}, color, debugName);
    uiDrawRect({r.x, r.y, 1.0f, r.h}, color, debugName);
    uiDrawRect({r.x + r.w - 1.0f, r.y, 1.0f, r.h}, color, debugName);
}

void uiDrawText(const char* text, float x, float y, float scale, glm::vec4 color,
                float italicShear)
{
    if (!text) return;

    if (fontReady())
    {
        // Convert from top-left coordinates to baseline coordinates.
        // All GUI callers pass y as the TOP of the text box.
        // The glyph renderer needs y as the BASELINE.
        float cursorX = x;
        float cursorY = y + (float)fontBase * scale;
        unsigned int prev = 0;

        for (const char* p = text; *p; ++p)
        {
            unsigned int ch = (unsigned char)*p;
            if (ch == '\n')
            {
                cursorX = x;
                cursorY += (float)fontLineHeight * scale;
                prev = 0;
                continue;
            }

            Glyph g{};
            if (!fontGetGlyph(ch, g))
            {
                cursorX += 12.0f * scale;
                prev = ch;
                continue;
            }

            cursorX += (float)fontGetKerning(prev, ch) * scale;
            float x0 = cursorX + (float)g.xoffset * scale;
            float y0 = cursorY - ((float)fontBase - (float)g.yoffset) * scale;
            float x1 = x0 + (float)g.w * scale;
            float y1 = y0 + (float)g.h * scale;

            float u0 = (float)g.x / (float)atlasWidth;
            float v0 = (float)g.y / (float)atlasHeight;
            float u1 = (float)(g.x + g.w) / (float)atlasWidth;
            float v1 = (float)(g.y + g.h) / (float)atlasHeight;

            GLuint pageTex = 0;
            if (g.page >= 0 && g.page < 8)
                pageTex = gFontPages[g.page];
            if (!pageTex)
            {
                uiDrawWarning("[MISSING FONT PAGE]", x0, y0);
                cursorX += (float)g.xadvance * scale;
                prev = ch;
                continue;
            }

            const float topShift = italicShear * (y1 - y0);
            float verts[] = {
                x0 + topShift,y0,u0,v0, x1 + topShift,y0,u1,v0, x1,y1,u1,v1,
                x0 + topShift,y0,u0,v0, x1,y1,u1,v1, x0,y1,u0,v1
            };
            const float textureMode = (g.page == 1) ? 2.0f : 1.0f;
            for (int i = 0; i < 6; ++i)
                gBatchVertices.push_back({{verts[i * 4], verts[i * 4 + 1]}, {verts[i * 4 + 2], verts[i * 4 + 3]}, color, textureMode});

            if (gDebug)
            {
                uiDrawRectOutline({x0, y0, x1 - x0, y1 - y0}, {0.2f, 1.0f, 0.2f, 0.55f}, "glyph-bounds");
                uiDrawRect({cursorX, cursorY, 18.0f * scale, 1.0f}, {1.0f, 0.2f, 0.2f, 0.7f}, "text-baseline");
            }

            cursorX += (float)g.xadvance * scale;
            prev = ch;
        }
        return;
    }

    float cursor = x;
    const float cell = 2.0f * scale;
    const float gap = 1.0f * scale;
    for (const char* p = text; *p; ++p) {
        int gi = glyphIndex(*p);
        if (gi < 0 || gi >= 44) gi = 37;
        for (int row = 0; row < 7; ++row) {
            unsigned char bits = FONT5X7[gi][row];
            for (int col = 0; col < 5; ++col) {
                if (bits & (1 << (4 - col))) {
                    uiDrawRect({cursor + col * (cell + gap), y + row * (cell + gap), cell, cell}, color, "text-pixel");
                }
            }
        }
        cursor += 6.0f * (cell + gap);
    }
}

void uiDrawImage(const char* path, UIRect r, glm::vec4 color)
{
    if (!path || r.w <= 0.0f || r.h <= 0.0f)
        return;
    GLuint texture = gTextures.getPath(path);
    if (!texture)
        return;
    float verts[] = {
        r.x, r.y, 0, 0, r.x + r.w, r.y, 1, 0, r.x + r.w, r.y + r.h, 1, 1,
        r.x, r.y, 0, 0, r.x + r.w, r.y + r.h, 1, 1, r.x, r.y + r.h, 0, 1
    };
    drawTexturedQuad(verts, 6, texture, color);
}

// Draw image scaled to fit within rect, preserving aspect ratio, centered, with optional checkerboard
void uiDrawImageFit(const char* path, UIRect r, bool checkerboard, glm::vec4 color)
{
    if (!path || r.w <= 0.0f || r.h <= 0.0f)
        return;
    GLuint texture = gTextures.getPath(path);
    if (!texture) {
        // Missing texture placeholder
        uiDrawRect(r, {0.15f, 0.05f, 0.05f, 1.0f}, "missing-tex-bg");
        uiDrawRectOutline(r, {0.8f, 0.2f, 0.2f, 1.0f}, "missing-tex-border");
        float x = r.x + r.w * 0.5f - 40.0f;
        float y = r.y + r.h * 0.5f - 8.0f;
        uiDrawText("?", x, y, 0.6f, {1.0f, 0.3f, 0.3f, 1.0f});
        uiDrawText("MISSING", r.x + 4.0f, r.y + r.h - 16.0f, 0.22f, {1.0f, 0.3f, 0.3f, 1.0f});
        return;
    }

    // Get texture dimensions
    GLint texW = 0, texH = 0;
    glBindTexture(GL_TEXTURE_2D, texture);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texW);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &texH);

    if (texW <= 0 || texH <= 0) {
        uiDrawRect(r, {0.15f, 0.05f, 0.05f, 1.0f}, "missing-tex-bg");
        uiDrawText("BAD TEX", r.x + 4.0f, r.y + r.h * 0.5f - 8.0f, 0.3f, {1.0f, 0.3f, 0.3f, 1.0f});
        return;
    }

    // Draw checkerboard background for transparency
    if (checkerboard) {
        int checkSize = 8;
        glm::vec4 c1 = {0.25f, 0.25f, 0.25f, 1.0f};
        glm::vec4 c2 = {0.35f, 0.35f, 0.35f, 1.0f};
        float sx = r.x, sy = r.y;
        for (float cy = sy; cy < sy + r.h; cy += checkSize) {
            for (float cx = sx; cx < sx + r.w; cx += checkSize) {
                int idx = (int)((cx - sx) / checkSize) + (int)((cy - sy) / checkSize);
                glm::vec4 c = (idx % 2 == 0) ? c1 : c2;
                float cw = std::min((float)checkSize, sx + r.w - cx);
                float ch = std::min((float)checkSize, sy + r.h - cy);
                float verts[] = {
                    cx, cy, 0,0, cx+cw, cy, 0,0, cx+cw, cy+ch, 0,0,
                    cx, cy, 0,0, cx+cw, cy+ch, 0,0, cx, cy+ch, 0,0
                };
                drawTriVerts(verts, 6, c, GL_TRIANGLES);
            }
        }
    }

    // Calculate aspect-ratio-preserving rect
    float scale = std::min(r.w / (float)texW, r.h / (float)texH);
    float drawW = (float)texW * scale;
    float drawH = (float)texH * scale;
    float drawX = r.x + (r.w - drawW) * 0.5f;
    float drawY = r.y + (r.h - drawH) * 0.5f;

    float verts[] = {
        drawX, drawY, 0, 0, drawX + drawW, drawY, 1, 0, drawX + drawW, drawY + drawH, 1, 1,
        drawX, drawY, 0, 0, drawX + drawW, drawY + drawH, 1, 1, drawX, drawY + drawH, 0, 1
    };
    drawTexturedQuad(verts, 6, texture, color);
}

void uiDrawMedia(const char* path, UIRect r, glm::vec4 color)
{
    if (!path || r.w <= 0.0f || r.h <= 0.0f) return;
    std::string p = path;
    size_t dot = p.rfind('.');
    if (dot == std::string::npos) { uiDrawImage(path, r, color); return; }
    std::string ext = p.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".gif") {
        const GifCache* gif = loadGif(path);
        if (!gif || gif->frames.empty()) return;
        GLuint tex = gif->frames[gif->currentFrame];
        if (!tex) return;
        float verts[] = {
            r.x, r.y, 0, 0, r.x + r.w, r.y, 1, 0, r.x + r.w, r.y + r.h, 1, 1,
            r.x, r.y, 0, 0, r.x + r.w, r.y + r.h, 1, 1, r.x, r.y + r.h, 0, 1
        };
        drawTexturedQuad(verts, 6, tex, color);
    } else {
        uiDrawImage(path, r, color);
    }
}

void uiUpdateMedia(float dt)
{
    updateAllGifs(dt);
}

void uiDrawImageRotated(const char* path, float cx, float cy, float halfSize, float angleDeg, glm::vec4 color)
{
    if (!path || halfSize <= 0.0f) return;
    GLuint texture = gTextures.getPath(path);
    if (!texture) return;

    float rad = angleDeg * 3.14159265f / 180.0f;
    float cosA = cosf(rad), sinA = sinf(rad);

    float verts[] = {
        cx + (-halfSize)*cosA - (-halfSize)*sinA, cy + (-halfSize)*sinA + (-halfSize)*cosA, 0, 0,
        cx + ( halfSize)*cosA - (-halfSize)*sinA, cy + ( halfSize)*sinA + (-halfSize)*cosA, 1, 0,
        cx + ( halfSize)*cosA - ( halfSize)*sinA, cy + ( halfSize)*sinA + ( halfSize)*cosA, 1, 1,
        cx + (-halfSize)*cosA - (-halfSize)*sinA, cy + (-halfSize)*sinA + (-halfSize)*cosA, 0, 0,
        cx + ( halfSize)*cosA - ( halfSize)*sinA, cy + ( halfSize)*sinA + ( halfSize)*cosA, 1, 1,
        cx + (-halfSize)*cosA - ( halfSize)*sinA, cy + (-halfSize)*sinA + ( halfSize)*cosA, 0, 1,
    };
    drawTexturedQuad(verts, 6, texture, color);
}

void uiDrawWarning(const char* text, float x, float y)
{
    uiDrawRect({x - 8.0f, y - 8.0f, 560.0f, 34.0f}, {0.6f, 0.0f, 0.0f, 0.85f}, "warning-bg");
    uiDrawText(text, x, y, 0.34f, {1.0f, 1.0f, 0.1f, 1.0f});
}
