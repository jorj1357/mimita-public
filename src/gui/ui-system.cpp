#include "ui-system.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include <glad/glad.h>
#include "gui/font-stuff/font-loader.h"
#include "debug/debug-log.h"
#include "debug/gl-debug.h"
#include "world/texture-store.h"
#include "gui/gui-media.h"

// yay sounds 6 4 2026 
#include "audio/audio.h"

namespace {
GLFWwindow* gWindow = nullptr;
GLuint gProgram = 0;
GLuint gVao = 0;
GLuint gVbo = 0;
GLint gScreenLoc = -1;
GLint gColorLoc = -1;
GLint gUseTexLoc = -1;
GLint gTexLoc = -1;
int gFbW = 1;
int gFbH = 1;
bool gDebug = false;
bool gMousePrev = false;
bool gMouseDown = false;
bool gMouseClickEdge = false;
bool gUiEditMode = false;
int gFrame = 0;
int gDrawCalls = 0;
int gWidgets = 0;
std::vector<std::string> gWarnings;

// Widget tracking for GUI editor
std::vector<UITrackedWidget> gTrackedWidgets;

// Global hover ownership: exactly one widget owns hover focus per frame.
// gHoverOwnerKey is set to the topmost (last-drawn) widget under the cursor.
static std::string gHoverOwnerKey;
static std::string gPrevHoverOwnerKey;

// Overlap debug visualization
static bool gOverlapDebugEnabled = false;

bool uiCanPlayUISound() {
    if (!gWindow) return false;
    if (glfwGetWindowAttrib(gWindow, GLFW_FOCUSED) == 0) return false;
    if (glfwGetWindowAttrib(gWindow, GLFW_ICONIFIED) != 0) return false;
    if (glfwGetInputMode(gWindow, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) return false;
    return true;
}

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

void ensureProgram()
{
    if (gProgram) return;

    printf("[UI] Initializing dedicated screen-space UI shader\n");
    const char* vs =
        "#version 330 core\n"
        "layout(location=0) in vec2 aPos;\n"
        "layout(location=1) in vec2 aUV;\n"
        "uniform vec2 uScreen;\n"
        "out vec2 vUV;\n"
        "void main(){\n"
        "  vec2 ndc = vec2((aPos.x/uScreen.x)*2.0-1.0, 1.0-(aPos.y/uScreen.y)*2.0);\n"
        "  vUV = aUV;\n"
        "  gl_Position = vec4(ndc, 0.0, 1.0);\n"
        "}\n";
    const char* fs =
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "in vec2 vUV;\n"
        "uniform vec4 uColor;\n"
        "uniform sampler2D uTex;\n"
        "uniform int uUseTex;\n"
        "void main(){\n"
        "  if (uUseTex == 1) {\n"
        "    vec4 texel = texture(uTex, vUV);\n"
        // "    FragColor = vec4(uColor.rgb, uColor.a * texel.a);\n"
        "FragColor = vec4(texel.rgb * uColor.rgb, texel.a * uColor.a);\n"
        // 5 23 2026 124 testing making buttons appear actually
        // "FragColor = vec4(uColor.rgb, texel.a * uColor.a);\n"
        "  } else {\n"
        "    FragColor = uColor;\n"
        "  }\n"
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
    gColorLoc = glGetUniformLocation(gProgram, "uColor");
    gUseTexLoc = glGetUniformLocation(gProgram, "uUseTex");
    gTexLoc = glGetUniformLocation(gProgram, "uTex");

    MIMITA_GL_CALL(glGenVertexArrays(1, &gVao));
    MIMITA_GL_CALL(glGenBuffers(1, &gVbo));
    MIMITA_GL_CALL(glBindVertexArray(gVao));
    MIMITA_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, gVbo));
    MIMITA_GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 128, nullptr, GL_DYNAMIC_DRAW));
    MIMITA_GL_CALL(glEnableVertexAttribArray(0));
    MIMITA_GL_CALL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)0));
    MIMITA_GL_CALL(glEnableVertexAttribArray(1));
    MIMITA_GL_CALL(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2)));
}

bool pointIn(double mx, double my, UIRect r)
{
    return mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h;
}

void drawTriVerts(const float* verts, int vertCount, glm::vec4 color, GLenum mode)
{
    ensureProgram();
    if (!gProgram || !gVao || !gVbo || !verts || vertCount <= 0)
        return;
    MIMITA_GL_CALL(glUseProgram(gProgram));
    MIMITA_GL_CALL(glUniform2f(gScreenLoc, (float)gFbW, (float)gFbH));
    MIMITA_GL_CALL(glUniform4fv(gColorLoc, 1, &color.x));
    MIMITA_GL_CALL(glUniform1i(gUseTexLoc, 0));
    MIMITA_GL_CALL(glBindVertexArray(gVao));
    MIMITA_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, gVbo));
    MIMITA_GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 4 * vertCount, verts, GL_DYNAMIC_DRAW));
    MIMITA_GL_CALL(glDrawArrays(mode, 0, vertCount));
    ++gDrawCalls;
}

void drawTexturedQuad(const float* verts, int vertCount, GLuint tex, glm::vec4 color)
{
    ensureProgram();
    if (!gProgram || !gVao || !gVbo || !verts || !tex || vertCount <= 0)
        return;
    MIMITA_GL_CALL(glUseProgram(gProgram));
    MIMITA_GL_CALL(glUniform2f(gScreenLoc, (float)gFbW, (float)gFbH));
    MIMITA_GL_CALL(glUniform4fv(gColorLoc, 1, &color.x));
    MIMITA_GL_CALL(glUniform1i(gUseTexLoc, 1));
    MIMITA_GL_CALL(glUniform1i(gTexLoc, 0));
    MIMITA_GL_CALL(glActiveTexture(GL_TEXTURE0));
    MIMITA_GL_CALL(glBindTexture(GL_TEXTURE_2D, tex));
    MIMITA_GL_CALL(glBindVertexArray(gVao));
    MIMITA_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, gVbo));
    MIMITA_GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 4 * vertCount, verts, GL_DYNAMIC_DRAW));
    MIMITA_GL_CALL(glDrawArrays(GL_TRIANGLES, 0, vertCount));
    ++gDrawCalls;
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
}

void uiInit(GLFWwindow* win)
{
    gWindow = win;
    printf("[UI] Initializing UI system\n");
    printf("[UI] Using built-in rectangle/vector fallback font; asset fonts are optional\n");
    ensureProgram();
}

void uiBeginFrame(GLFWwindow* win, const char* passName)
{
    gWindow = win;
    glfwGetFramebufferSize(win, &gFbW, &gFbH);
    if (gFbW <= 0) gFbW = 1;
    if (gFbH <= 0) gFbH = 1;

    ++gFrame;
    gDrawCalls = 0;
    gWidgets = 0;
    gTrackedWidgets.clear();
    gHoverOwnerKey.clear();
    gMouseDown = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    gMouseClickEdge = gMouseDown && !gMousePrev;

    MIMITA_GL_CLEAR_STAGE("uiBeginFrame");
    MIMITA_GL_CALL(glViewport(0, 0, gFbW, gFbH));
    MIMITA_GL_CALL(glDisable(GL_DEPTH_TEST));
    MIMITA_GL_CALL(glEnable(GL_BLEND));
    MIMITA_GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    MIMITA_GL_CALL(glDisable(GL_CULL_FACE));
    ensureProgram();
    if (gProgram)
        MIMITA_GL_CALL(glUseProgram(gProgram));

    if (gFrame % 120 == 1)
        Debug::logThrottled(Debug::Category::Render, "ui-frame", DebugConfig::PRINT_INTERVAL, "[UI] Rendering UI frame %d pass=%s framebuffer=%dx%d\n", gFrame, passName ? passName : "unknown", gFbW, gFbH);
}

static void drawOverlapDebug()
{
    if (!gOverlapDebugEnabled || gTrackedWidgets.empty()) return;

    for (size_t i = 0; i < gTrackedWidgets.size(); ++i)
    {
        const UITrackedWidget& a = gTrackedWidgets[i];
        for (size_t j = i + 1; j < gTrackedWidgets.size(); ++j)
        {
            const UITrackedWidget& b = gTrackedWidgets[j];
            const UIRect& ra = a.rect;
            const UIRect& rb = b.rect;

            if (ra.x < rb.x + rb.w && ra.x + ra.w > rb.x &&
                ra.y < rb.y + rb.h && ra.y + ra.h > rb.y)
            {
                float ox = std::max(ra.x, rb.x);
                float oy = std::max(ra.y, rb.y);
                float ow = std::min(ra.x + ra.w, rb.x + rb.w) - ox;
                float oh = std::min(ra.y + ra.h, rb.y + rb.h) - oy;

                printf("[GUI OVERLAP] \"%s\" overlaps \"%s\"  overlap=(%.0f,%.0f,%.0f,%.0f)\n",
                       a.id.c_str(), b.id.c_str(), ox, oy, ow, oh);

                // Draw overlapping region in semi-transparent red
                uiDrawRect({ox, oy, ow, oh}, {1.0f, 0.0f, 0.0f, 0.35f}, "overlap-debug");

                // Draw outlines around both overlapping widgets
                uiDrawRectOutline(ra, {1.0f, 0.0f, 0.0f, 0.8f}, "overlap-widget");
                uiDrawRectOutline(rb, {1.0f, 0.0f, 0.0f, 0.8f}, "overlap-widget");
            }
        }
    }
}

void uiEndFrame()
{
    // Global hover ownership: determine enter/exit for the topmost widget only
    if (gHoverOwnerKey != gPrevHoverOwnerKey)
    {
        if (!gPrevHoverOwnerKey.empty())
        {
            printf("[UI HOVER EXIT] id=%s\n", gPrevHoverOwnerKey.c_str());
        }
        if (!gHoverOwnerKey.empty())
        {
            printf("[UI HOVER ENTER] id=%s\n", gHoverOwnerKey.c_str());
            if (uiCanPlayUISound()) {
                playMenuHover();
            }
        }
        gPrevHoverOwnerKey = gHoverOwnerKey;
    }

    // Overlap debug visualization (drawn before GL state cleanup)
    drawOverlapDebug();

    if (gFrame % 120 == 1)
        Debug::logThrottled(Debug::Category::Render, "ui-frame-complete", DebugConfig::PRINT_INTERVAL, "[UI] Render pass complete drawCalls=%d widgets=%d warnings=%zu\n", gDrawCalls, gWidgets, gWarnings.size());
    gMousePrev = gMouseDown;
    MIMITA_GL_CALL(glUseProgram(0));
    MIMITA_GL_CALL(glEnable(GL_DEPTH_TEST));
}

// 5 23 2026 idk where to put this 
float uiMeasureText(const char* text, float scale)
{
    float w = 0.0f;

    for (const char* p = text; *p; ++p)
    {
        Glyph g{};
        if (fontGetGlyph((unsigned char)*p, g))
            w += g.xadvance * scale;
    }

    return w;
}

void uiSetDebug(bool enabled) { gDebug = enabled; }
void uiSetEditMode(bool enabled) { gUiEditMode = enabled; }
bool uiEditModeEnabled() { return gUiEditMode; }
bool uiDebugEnabled() { return gDebug; }
void uiSetOverlapDebug(bool enabled) { gOverlapDebugEnabled = enabled; }
bool uiOverlapDebugEnabled() { return gOverlapDebugEnabled; }

const std::vector<UITrackedWidget>& uiGetTrackedWidgets()
{
    return gTrackedWidgets;
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

void uiDrawRectOutline(UIRect r, glm::vec4 color, const char* debugName)
{
    (void)debugName;
    float verts[] = {
        r.x, r.y, 0, 0, r.x + r.w, r.y, 0, 0,
        r.x + r.w, r.y, 0, 0, r.x + r.w, r.y + r.h, 0, 0,
        r.x + r.w, r.y + r.h, 0, 0, r.x, r.y + r.h, 0, 0,
        r.x, r.y + r.h, 0, 0, r.x, r.y, 0, 0
    };
    drawTriVerts(verts, 8, color, GL_LINES);
}

void uiDrawText(const char* text, float x, float y, float scale, glm::vec4 color)
{
    if (!text) return;

    if (fontReady())
    {
        float cursorX = x;
        float cursorY = y;
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

            // printf(
            //     "[GLYPH] ch=%c x=%d y=%d w=%d h=%d xoff=%d yoff=%d xadv=%d page=%d\n",
            //     ch,
            //     g.x,
            //     g.y,
            //     g.w,
            //     g.h,
            //     g.xoffset,
            //     g.yoffset,
            //     g.xadvance,
            //     g.page
            // );

            cursorX += (float)fontGetKerning(prev, ch) * scale;
            float x0 = cursorX + (float)g.xoffset * scale;

            // 5 23 2026 working on font scale might not work 
            // float y0 = cursorY + (float)g.yoffset * scale;
            // float y0 = cursorY + ((float)g.yoffset - (float)fontBase) * scale;
            // float baseline = cursorY + fontBase * scale;
            // float y0 = baseline - ((float)fontBase - (float)g.yoffset) * scale;

            // 5 23 2026 attempt 4 font scale fix 
            // float y0 = cursorY + (float)g.yoffset * scale;
            // 5 23 2026 attemtp 5 118 pm
            float y0 = cursorY - ((float)fontBase - (float)g.yoffset) * scale;

            // 5 23 2026 try this if above line not wokring 
            // float y0 = cursorY + ((float)fontBase - (float)g.yoffset) * scale;

            float x1 = x0 + (float)g.w * scale;
            float y1 = y0 + (float)g.h * scale;

            // 5 23 2026 also rounding? test? for fonts 
            // x0 = roundf(x0);
            // y0 = roundf(y0);
            // x1 = roundf(x1);
            // y1 = roundf(y1);

            float u0 = (float)g.x / (float)atlasWidth;
            float v0 = (float)g.y / (float)atlasHeight;
            float u1 = (float)(g.x + g.w) / (float)atlasWidth;
            float v1 = (float)(g.y + g.h) / (float)atlasHeight;

            // 5 23 2026 fixing block texures idk 
            // float v0 = 1.0f - ((float)(g.y + g.h) / (float)atlasHeight);
            // float v1 = 1.0f - ((float)g.y / (float)atlasHeight);

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

            float verts[] = {
                x0,y0,u0,v0, x1,y0,u1,v0, x1,y1,u1,v1,
                x0,y0,u0,v0, x1,y1,u1,v1, x0,y1,u0,v1
            };
            drawTexturedQuad(verts, 6, pageTex, color);

            if (gDebug)
            {
                uiDrawRectOutline({x0, y0, x1 - x0, y1 - y0}, {0.2f, 1.0f, 0.2f, 0.55f}, "glyph-bounds");
                uiDrawRect({cursorX, cursorY + (float)fontBase * scale, 18.0f * scale, 1.0f}, {1.0f, 0.2f, 0.2f, 0.7f}, "text-baseline");
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
        // PNG/JPG fall through to existing image loader
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

    // 4 corners of a square centered at (cx,cy), rotated by angleDeg
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

UIButtonState uiButton(GLFWwindow* win, const char* text, UIRect r, glm::vec4 color, const char* id)
{
    ++gWidgets;
    double mx = 0.0, my = 0.0;
    glfwGetCursorPos(win, &mx, &my);
    UIButtonState s;
    const bool rawHovered = pointIn(mx, my, r);
    s.pressed = rawHovered && gMouseDown;
    // In edit mode, buttons never fire — the editor consumes all clicks
    s.clicked = !gUiEditMode && rawHovered && gMouseClickEdge;

    const char* key = id ? id : text;

    // Global hover ownership: the last (topmost) widget under cursor wins.
    s.hovered = rawHovered;
    if (rawHovered) {
        gHoverOwnerKey = key;
    }

    glm::vec4 c = color;
    if (s.hovered) c += glm::vec4(0.14f, 0.14f, 0.14f, 0.0f);
    if (s.pressed) c *= glm::vec4(0.75f, 0.75f, 0.75f, 1.0f);
    uiDrawRect(r, c, text);
    uiDrawRectOutline(r, {1.0f, 1.0f, 1.0f, 0.85f}, "button-border");

    // float textScale = std::clamp(r.h / 110.0f, 0.38f, 0.62f);
    float textScale = std::clamp(r.h / 110.0f, 0.38f, 1.2f);
    // float textScale = 1.0f;
    // float textScale = 0.5f;
    // float textScale = 0.1f;
    // float textW = (float)std::strlen(text) * 24.0f * textScale;
    float textW = uiMeasureText(text, textScale);
    // uiDrawText(text, r.x + (r.w - textW) * 0.5f, r.y + r.h * 0.34f, textScale, {0.02f, 0.02f, 0.025f, 1.0f});
    uiDrawText(text, r.x + (r.w - textW) * 0.5f, r.y + r.h * 0.34f, textScale, {1.0f, 1.0f, 1.0f, 1.0f});
    debugWidget("BUTTON", text, r, s.hovered, s.pressed);
    gTrackedWidgets.push_back({key, r, s.hovered, s.pressed});

    if (s.clicked)
    {
        printf("[UI] button clicked: %s\n", text);
        if (uiCanPlayUISound()) {
            playMenuClick();
        }
    }

    return s;
}

bool uiCheckbox(GLFWwindow* win, const char* label, UIRect r, bool* value)
{
    UIButtonState s = uiButton(win, *value ? "ON" : "OFF", r, *value ? glm::vec4(0.2f,0.8f,0.35f,1) : glm::vec4(0.7f,0.25f,0.25f,1));
    uiDrawText(label, r.x + r.w + 14.0f, r.y + 10.0f, 0.42f, {0.88f,0.9f,0.94f,1});
    if (s.clicked) *value = !*value;
    debugWidget("CHECKBOX", label, r, s.hovered, s.pressed);
    return s.clicked;
}

bool uiSlider(GLFWwindow* win, const char* label, UIRect r, float* value, float minValue, float maxValue)
{
    ++gWidgets;
    double mx = 0.0, my = 0.0;
    glfwGetCursorPos(win, &mx, &my);
    bool hovered = pointIn(mx, my, r);
    if (!gUiEditMode && hovered && gMouseDown) {
        float t = std::clamp((float(mx) - r.x) / r.w, 0.0f, 1.0f);
        *value = minValue + t * (maxValue - minValue);
    }

    float t = (*value - minValue) / (maxValue - minValue);
    uiDrawRect(r, {0.12f,0.14f,0.18f,1}, "slider-track");
    uiDrawRect({r.x, r.y, r.w * t, r.h}, {0.25f,0.65f,0.95f,1}, "slider-fill");
    uiDrawRectOutline(r, {0.9f,0.9f,0.9f,0.9f}, "slider-border");
    char buf[128];
    snprintf(buf, sizeof(buf), "%s %.1f", label, *value);
    uiDrawText(buf, r.x, r.y - 28.0f, 0.42f, {0.88f,0.9f,0.94f,1});
    debugWidget("SLIDER", label, r, hovered, hovered && gMouseDown);
    return !gUiEditMode && hovered && gMouseDown;
}

void uiPlaceholderImageButton(GLFWwindow* win, const char* label, UIRect r)
{
    UIButtonState s = uiButton(win, "IMG", r, {0.35f,0.24f,0.65f,1});
    uiDrawRect({r.x + 10, r.y + 10, r.w - 20, r.h - 20}, {0.95f,0.2f,0.85f,0.35f}, "missing-image");
    uiDrawText("[MISSING TEXTURE]", r.x + 8, r.y + r.h + 8, 0.35f, {1.0f,0.8f,0.2f,1});
    debugWidget("IMAGE_BUTTON", label, r, s.hovered, s.pressed);
}

float uiScreenW()
{
    return (float)gFbW;
}

float uiScreenH()
{
    return (float)gFbH;
}

float uiScaleX(float px)
{
    return px * ((float)gFbW / 1920.0f);
}

float uiScaleY(float px)
{
    return px * ((float)gFbH / 1080.0f);
}

UIRect uiCentered(float w, float h, float y)
{
    return {
        uiScreenW() * 0.5f - w * 0.5f,
        y,
        w,
        h
    };
}

UIRect uiRow(
    float x,
    float& y,
    float w,
    float h,
    float gap
)
{
    UIRect r{
        x,
        y,
        w,
        h
    };

    y += h + gap;

    return r;
}

void uiRenderFrameDebugOverlay(GLFWwindow* win, const char* activeScene, bool worldPassRan)
{
    (void)win;
    if (!gDebug) return;
    char buf[256];
    snprintf(buf, sizeof(buf), "FPS/FRAME UI PASS OK | scene=%s worldPass=%s framebuffer=%dx%d drawCalls=%d widgets=%d",
             activeScene, worldPassRan ? "ran" : "skipped", gFbW, gFbH, gDrawCalls, gWidgets);
    uiDrawRect({10, 10, 760, 54}, {0.0f, 0.0f, 0.0f, 0.62f}, "debug-overlay-bg");
    uiDrawText(buf, 18, 24, 0.32f, {0.55f, 1.0f, 0.65f, 1.0f});
}
