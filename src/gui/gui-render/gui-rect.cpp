// C:\important\quiet\n\mimita-priv-v7\src\gui\gui-render\gui-rect.cpp
// mar 8 2026
/**
 * purpose
 * expose ONE function:
 * drawGuiRect(args)
 *
 * this file DOES:
 * - draw one gui rect in normalized device coordinates
 *
 * this file DOES NOT:
 * - know about buttons or menus
 */

#include "gui-rect.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>

static GLuint vao = 0;
static GLuint vbo = 0;

void drawGuiRect(
    float x,
    float y,
    float w,
    float h,
    glm::vec4 color
)
{
    printf("[GUI RECT] begin\n");
    printf("[GUI RECT] pixel rect x=%f y=%f w=%f h=%f\n", x, y, w, h);

    GLFWwindow* win = glfwGetCurrentContext();
    if (!win)
    {
        printf("[GUI RECT] no current GL window\n");
        return;
    }

    int screenW = 0;
    int screenH = 0;
    glfwGetFramebufferSize(win, &screenW, &screenH);

    if (screenW <= 0 || screenH <= 0)
    {
        printf("[GUI RECT] bad framebuffer size\n");
        return;
    }

    float x0 = (x / screenW) * 2.0f - 1.0f;
    float y0 = 1.0f - (y / screenH) * 2.0f;

    float x1 = ((x + w) / screenW) * 2.0f - 1.0f;
    float y1 = 1.0f - ((y + h) / screenH) * 2.0f;

    printf("[GUI RECT] ndc rect x0=%f y0=%f x1=%f y1=%f\n", x0, y0, x1, y1);

    float verts[] = {
        x0, y0,
        x1, y0,
        x1, y1,

        x0, y0,
        x1, y1,
        x0, y1
    };

    if (!vao)
    {
        printf("[GUI RECT] create vao/vbo\n");

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    }

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    
    // mar 14 2026 
    // this happens per letter, per glyph etc
    // too much performance cost
    // batch later todo
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

    glDisable(GL_DEPTH_TEST);

    // note:
    // this currently assumes you already have a very basic flat-color shader bound.
    // if nothing appears, next thing to add is a dedicated gui shader.
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glEnable(GL_DEPTH_TEST);

    printf("[GUI RECT] end\n");
}