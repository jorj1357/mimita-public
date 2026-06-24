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
        "FragColor = vec4(texel.rgb * uColor.rgb, texel.a * uColor.a);\n"
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

UIButtonState uiButton(GLFWwindow* win, const char* text, UIRect r, glm::vec4 color, const char* id)
{
    ++gWidgets;

    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    UIRect fbR = cs.designToScreen(r);

    double mx = 0.0, my = 0.0;
    glfwGetCursorPos(win, &mx, &my);
    double fbx = mx, fby = my;
    cs.cursorWindowToScreen(mx, my, fbx, fby);

    UIButtonState s;
    const bool rawHovered = pointIn(fbx, fby, fbR);
    s.pressed = rawHovered && gMouseDown;
    s.clicked = !gUiEditMode && rawHovered && gMouseClickEdge;

    const char* key = id ? id : text;

    s.hovered = rawHovered;
    if (rawHovered) {
        gHoverOwnerKey = key;
    }

    glm::vec4 c = color;
    if (s.hovered) c += glm::vec4(0.14f, 0.14f, 0.14f, 0.0f);
    if (s.pressed) c *= glm::vec4(0.75f, 0.75f, 0.75f, 1.0f);
    uiDrawRect(fbR, c, text);
    uiDrawRectOutline(fbR, {1.0f, 1.0f, 1.0f, 0.85f}, "button-border");

    float textScale = std::clamp(fbR.h / 110.0f, 0.38f, 1.2f);
    float textW = uiMeasureText(text, textScale);
    uiDrawText(text, fbR.x + (fbR.w - textW) * 0.5f, fbR.y + fbR.h * 0.34f, textScale, {1.0f, 1.0f, 1.0f, 1.0f});
    debugWidget("BUTTON", text, fbR, s.hovered, s.pressed);
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

    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    UIRect fbR = cs.designToScreen(r);

    double mx = 0.0, my = 0.0;
    glfwGetCursorPos(win, &mx, &my);
    double fbx = mx, fby = my;
    cs.cursorWindowToScreen(mx, my, fbx, fby);

    bool hovered = pointIn(fbx, fby, fbR);
    if (!gUiEditMode && hovered && gMouseDown) {
        float t = std::clamp((float(fbx) - fbR.x) / fbR.w, 0.0f, 1.0f);
        *value = minValue + t * (maxValue - minValue);
    }

    float t = (*value - minValue) / (maxValue - minValue);
    uiDrawRect(fbR, {0.12f,0.14f,0.18f,1}, "slider-track");
    uiDrawRect({fbR.x, fbR.y, fbR.w * t, fbR.h}, {0.25f,0.65f,0.95f,1}, "slider-fill");
    uiDrawRectOutline(fbR, {0.9f,0.9f,0.9f,0.9f}, "slider-border");
    char buf[128];
    snprintf(buf, sizeof(buf), "%s %.1f", label, *value);
    uiDrawText(buf, fbR.x, fbR.y - 28.0f, 0.42f, {0.88f,0.9f,0.94f,1});
    debugWidget("SLIDER", label, fbR, hovered, hovered && gMouseDown);
    return !gUiEditMode && hovered && gMouseDown;
}

void uiPlaceholderImageButton(GLFWwindow* win, const char* label, UIRect r)
{
    UIButtonState s = uiButton(win, "IMG", r, {0.35f,0.24f,0.65f,1});
    uiDrawRect({r.x + 10, r.y + 10, r.w - 20, r.h - 20}, {0.95f,0.2f,0.85f,0.35f}, "missing-image");
    uiDrawText("[MISSING TEXTURE]", r.x + 8, r.y + r.h + 8, 0.35f, {1.0f,0.8f,0.2f,1});
    debugWidget("IMAGE_BUTTON", label, r, s.hovered, s.pressed);
}
