#include "ui-system.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <glad/glad.h>

namespace {
GLFWwindow* gWindow = nullptr;
GLuint gProgram = 0;
GLuint gVao = 0;
GLuint gVbo = 0;
GLint gScreenLoc = -1;
GLint gColorLoc = -1;
int gFbW = 1;
int gFbH = 1;
bool gDebug = true;
bool gMousePrev = false;
bool gMouseDown = false;
bool gMouseClickEdge = false;
int gFrame = 0;
int gDrawCalls = 0;
int gWidgets = 0;
std::vector<std::string> gWarnings;

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
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
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
        "uniform vec2 uScreen;\n"
        "void main(){\n"
        "  vec2 ndc = vec2((aPos.x/uScreen.x)*2.0-1.0, 1.0-(aPos.y/uScreen.y)*2.0);\n"
        "  gl_Position = vec4(ndc, 0.0, 1.0);\n"
        "}\n";
    const char* fs =
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "uniform vec4 uColor;\n"
        "void main(){ FragColor = uColor; }\n";

    GLuint vert = compile(GL_VERTEX_SHADER, vs, "ui.vert");
    GLuint frag = compile(GL_FRAGMENT_SHADER, fs, "ui.frag");
    gProgram = glCreateProgram();
    glAttachShader(gProgram, vert);
    glAttachShader(gProgram, frag);
    glLinkProgram(gProgram);
    glDeleteShader(vert);
    glDeleteShader(frag);

    GLint ok = 0;
    glGetProgramiv(gProgram, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(gProgram, sizeof(log), nullptr, log);
        printf("[UI ERROR] program link failed\n%s\n", log);
    } else {
        printf("[UI] UI shader linked program=%u\n", gProgram);
    }

    gScreenLoc = glGetUniformLocation(gProgram, "uScreen");
    gColorLoc = glGetUniformLocation(gProgram, "uColor");

    glGenVertexArrays(1, &gVao);
    glGenBuffers(1, &gVbo);
    glBindVertexArray(gVao);
    glBindBuffer(GL_ARRAY_BUFFER, gVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 64, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);
}

bool pointIn(double mx, double my, UIRect r)
{
    return mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h;
}

void drawTriVerts(const float* verts, int vertCount, glm::vec4 color, GLenum mode)
{
    ensureProgram();
    glUseProgram(gProgram);
    glUniform2f(gScreenLoc, (float)gFbW, (float)gFbH);
    glUniform4fv(gColorLoc, 1, &color.x);
    glBindVertexArray(gVao);
    glBindBuffer(GL_ARRAY_BUFFER, gVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 2 * vertCount, verts, GL_DYNAMIC_DRAW);
    glDrawArrays(mode, 0, vertCount);
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
    uiDrawText(buf, r.x, r.y + r.h + 8.0f, 1.2f, {0.7f, 1.0f, 0.95f, 1.0f});
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
    gMouseDown = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    gMouseClickEdge = gMouseDown && !gMousePrev;

    glViewport(0, 0, gFbW, gFbH);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    ensureProgram();
    glUseProgram(gProgram);

    if (gFrame % 120 == 1)
        printf("[UI] Rendering UI frame %d pass=%s framebuffer=%dx%d\n", gFrame, passName ? passName : "unknown", gFbW, gFbH);
}

void uiEndFrame()
{
    if (gFrame % 120 == 1)
        printf("[UI] Render pass complete drawCalls=%d widgets=%d warnings=%zu\n", gDrawCalls, gWidgets, gWarnings.size());
    gMousePrev = gMouseDown;
    glUseProgram(0);
    glEnable(GL_DEPTH_TEST);
}

void uiSetDebug(bool enabled) { gDebug = enabled; }
bool uiDebugEnabled() { return gDebug; }

void uiDrawRect(UIRect r, glm::vec4 color, const char* debugName)
{
    if (r.w <= 0.0f || r.h <= 0.0f) {
        printf("[UI WARNING] bad rect %s %.1f %.1f %.1f %.1f\n", debugName ? debugName : "unnamed", r.x, r.y, r.w, r.h);
        return;
    }
    float verts[] = {
        r.x, r.y, r.x + r.w, r.y, r.x + r.w, r.y + r.h,
        r.x, r.y, r.x + r.w, r.y + r.h, r.x, r.y + r.h
    };
    drawTriVerts(verts, 6, color, GL_TRIANGLES);
}

void uiDrawRectOutline(UIRect r, glm::vec4 color, const char* debugName)
{
    (void)debugName;
    float verts[] = {
        r.x, r.y, r.x + r.w, r.y,
        r.x + r.w, r.y, r.x + r.w, r.y + r.h,
        r.x + r.w, r.y + r.h, r.x, r.y + r.h,
        r.x, r.y + r.h, r.x, r.y
    };
    drawTriVerts(verts, 8, color, GL_LINES);
}

void uiDrawText(const char* text, float x, float y, float scale, glm::vec4 color)
{
    if (!text) return;

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

void uiDrawWarning(const char* text, float x, float y)
{
    uiDrawRect({x - 8.0f, y - 8.0f, 560.0f, 34.0f}, {0.6f, 0.0f, 0.0f, 0.85f}, "warning-bg");
    uiDrawText(text, x, y, 1.3f, {1.0f, 1.0f, 0.1f, 1.0f});
}

UIButtonState uiButton(GLFWwindow* win, const char* text, UIRect r, glm::vec4 color)
{
    ++gWidgets;
    double mx = 0.0, my = 0.0;
    glfwGetCursorPos(win, &mx, &my);
    UIButtonState s;
    s.hovered = pointIn(mx, my, r);
    s.pressed = s.hovered && gMouseDown;
    s.clicked = s.hovered && gMouseClickEdge;

    glm::vec4 c = color;
    if (s.hovered) c += glm::vec4(0.14f, 0.14f, 0.14f, 0.0f);
    if (s.pressed) c *= glm::vec4(0.75f, 0.75f, 0.75f, 1.0f);
    uiDrawRect(r, c, text);
    uiDrawRectOutline(r, {1.0f, 1.0f, 1.0f, 0.85f}, "button-border");

    float textScale = std::max(1.4f, r.h / 24.0f);
    float textW = (float)std::strlen(text) * 6.0f * (2.0f * textScale + 1.0f * textScale);
    uiDrawText(text, r.x + (r.w - textW) * 0.5f, r.y + r.h * 0.34f, textScale, {0.02f, 0.02f, 0.025f, 1.0f});
    debugWidget("BUTTON", text, r, s.hovered, s.pressed);

    if (s.clicked)
        printf("[UI] button clicked: %s\n", text);
    return s;
}

bool uiCheckbox(GLFWwindow* win, const char* label, UIRect r, bool* value)
{
    UIButtonState s = uiButton(win, *value ? "ON" : "OFF", r, *value ? glm::vec4(0.2f,0.8f,0.35f,1) : glm::vec4(0.7f,0.25f,0.25f,1));
    uiDrawText(label, r.x + r.w + 14.0f, r.y + 10.0f, 1.5f, {0.88f,0.9f,0.94f,1});
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
    if (hovered && gMouseDown) {
        float t = std::clamp((float(mx) - r.x) / r.w, 0.0f, 1.0f);
        *value = minValue + t * (maxValue - minValue);
    }

    float t = (*value - minValue) / (maxValue - minValue);
    uiDrawRect(r, {0.12f,0.14f,0.18f,1}, "slider-track");
    uiDrawRect({r.x, r.y, r.w * t, r.h}, {0.25f,0.65f,0.95f,1}, "slider-fill");
    uiDrawRectOutline(r, {0.9f,0.9f,0.9f,0.9f}, "slider-border");
    char buf[128];
    snprintf(buf, sizeof(buf), "%s %.1f", label, *value);
    uiDrawText(buf, r.x, r.y - 28.0f, 1.35f, {0.88f,0.9f,0.94f,1});
    debugWidget("SLIDER", label, r, hovered, hovered && gMouseDown);
    return hovered && gMouseDown;
}

void uiPlaceholderImageButton(GLFWwindow* win, const char* label, UIRect r)
{
    UIButtonState s = uiButton(win, "IMG", r, {0.35f,0.24f,0.65f,1});
    uiDrawRect({r.x + 10, r.y + 10, r.w - 20, r.h - 20}, {0.95f,0.2f,0.85f,0.35f}, "missing-image");
    uiDrawText("[MISSING TEXTURE]", r.x + 8, r.y + r.h + 8, 1.15f, {1.0f,0.8f,0.2f,1});
    debugWidget("IMAGE_BUTTON", label, r, s.hovered, s.pressed);
}

void uiRenderFrameDebugOverlay(GLFWwindow* win, const char* activeScene, bool worldPassRan)
{
    (void)win;
    if (!gDebug) return;
    char buf[256];
    snprintf(buf, sizeof(buf), "FPS/FRAME UI PASS OK | scene=%s worldPass=%s framebuffer=%dx%d drawCalls=%d widgets=%d",
             activeScene, worldPassRan ? "ran" : "skipped", gFbW, gFbH, gDrawCalls, gWidgets);
    uiDrawRect({10, 10, 760, 54}, {0.0f, 0.0f, 0.0f, 0.62f}, "debug-overlay-bg");
    uiDrawText(buf, 18, 24, 1.15f, {0.55f, 1.0f, 0.65f, 1.0f});
}
