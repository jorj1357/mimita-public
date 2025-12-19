// C:\important\go away v5\s\mimita-v5\src\renderer\renderer.cpp

/*
todo nov 6 2025
combine with map renderer
*/

#define GLFW_INCLUDE_NONE
#define STB_TRUETYPE_IMPLEMENTATION
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "stb_truetype.h"

#include <algorithm>
#include <vector>

#include "renderer/renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

static GLuint shaderProgram = 0;
static GLuint cubeVAO = 0, cubeVBO = 0;
static GLuint groundVAO = 0, groundVBO = 0;

// use location 2 for vec2 not 1
static const char* vertexShaderSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 2) in vec2 aUV;
out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    TexCoord = aUV;
}
)";

/*
FragColor = texture(tex, TexCoord) * 1.5;
the * 1.5 is how we change brightness
for now
bc not good
i want it coded in here but later
later replace this with directional lighting.
todo nov 6 2025
*/
static const char* fragShaderSrc = R"(
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform vec3 color;
uniform sampler2D tex;
uniform bool useTex;

void main() {
    if (useTex)
        FragColor = texture(tex, TexCoord);
    else
        FragColor = vec4(color * 1.5, 1.0);
}
)";

static stbtt_bakedchar cdata[96]; // ASCII 32..126
static GLuint fontTex = 0;

static void initFont() {
    if (fontTex) return; // already loaded

    unsigned char ttfBuffer[1 << 20];
    unsigned char tempBitmap[512 * 512];
    FILE* f = fopen("C:\\Windows\\Fonts\\arial.ttf", "rb");
    if (!f) return;
    fread(ttfBuffer, 1, 1 << 20, f);
    fclose(f);

    stbtt_BakeFontBitmap(ttfBuffer, 0, 32.0f, tempBitmap, 512, 512, 32, 96, cdata);

    glGenTextures(1, &fontTex);
    glBindTexture(GL_TEXTURE_2D, fontTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 512, 512, 0, GL_RED, GL_UNSIGNED_BYTE, tempBitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

static GLuint makeShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    return s;
}

Renderer::Renderer(int w, int h, const char* title) {
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW.\n");
        return;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(w, h, title, nullptr, nullptr);
    if (!window) {
        fprintf(stderr, "Failed to create GLFW window.\n");
        glfwTerminate();
        return;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        fprintf(stderr, "Failed to init GLAD.\n");
        glfwTerminate();
        window = nullptr;
        return;
    }

    printf("OpenGL version: %s\n", glGetString(GL_VERSION));
    glEnable(GL_DEPTH_TEST);

    // ---- Shader creation with error checking ----
    GLuint vs = makeShader(GL_VERTEX_SHADER, vertexShaderSrc);
    GLuint fs = makeShader(GL_FRAGMENT_SHADER, fragShaderSrc);
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);

    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char info[1024];
        glGetProgramInfoLog(shaderProgram, sizeof(info), NULL, info);
        fprintf(stderr, "[Shader Link Error]\n%s\n", info);
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    // ---- end shader checks ----

    // ---- Cube (player) ----
    float cubeVerts[] = {
        -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f, -0.5f, 0.5f,-0.5f,
        -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f
    };
    unsigned int cubeIdx[] = {
        0,1,2,2,3,0, 4,5,6,6,7,4,
        0,1,5,5,4,0, 2,3,7,7,6,2,
        0,3,7,7,4,0, 1,2,6,6,5,1
    };

    GLuint cubeEBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glGenBuffers(1, &cubeEBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerts), cubeVerts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIdx), cubeIdx, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // ---- Ground plane ----
    // z up dec 19 2025 fix but i dont think we even use it
    float groundVerts[] = {
        -50.0f, -50.0f, 0.0f,
        50.0f, -50.0f, 0.0f,
        50.0f,  50.0f, 0.0f,
        -50.0f,  50.0f, 0.0f
    };
    unsigned int groundIdx[] = { 0,1,2, 2,3,0 };

    GLuint groundEBO;
    glGenVertexArrays(1, &groundVAO);
    glGenBuffers(1, &groundVBO);
    glGenBuffers(1, &groundEBO);
    glBindVertexArray(groundVAO);
    glBindBuffer(GL_ARRAY_BUFFER, groundVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(groundVerts), groundVerts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, groundEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(groundIdx), groundIdx, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    printf("Renderer initialized successfully.\n");
}

float Renderer::beginFrame() {
    static double last = glfwGetTime();
    double now = glfwGetTime();
    float dt = float(now - last);
    last = now;
    glClearColor(0.05f, 0.05f, 0.07f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    return dt;
}

/*
todo nov 6 2025
remove phase out
    drawCube
    drawCubeColored
remove these
*/
void Renderer::drawCube(const glm::vec3& pos, const glm::mat4& view, const glm::mat4& proj) {
    glUseProgram(shaderProgram);
    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"model"),1,GL_FALSE,&model[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"view"),1,GL_FALSE,&view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"projection"),1,GL_FALSE,&proj[0][0]);
    glUniform3f(glGetUniformLocation(shaderProgram,"color"), 0.1f, 0.8f, 1.0f);
    glBindVertexArray(cubeVAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    // double stop state leak
    glBindVertexArray(0);
    glUseProgram(0);
}

void Renderer::drawCubeColored(const glm::vec3& pos, const glm::vec3& color,
                               const glm::mat4& view, const glm::mat4& proj) {
    glUseProgram(shaderProgram);
    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"model"),1,GL_FALSE,&model[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"view"),1,GL_FALSE,&view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"projection"),1,GL_FALSE,&proj[0][0]);
    glUniform3fv(glGetUniformLocation(shaderProgram,"color"),1,&color[0]);
    glBindVertexArray(cubeVAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    // stop state leak
    glBindVertexArray(0);
    glUseProgram(0);
}

// dec 16 2025 todo make this file not full of random stuff i dont  think i need like cube
// and just reuse a object renderer for everthing 
void Renderer::drawDebugSphere(
    const glm::vec3& center,
    float radius,
    const glm::vec3& color,
    const glm::mat4& view,
    const glm::mat4& proj)
{
    glUseProgram(shaderProgram);

    // semi-transparent
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glm::mat4 model = glm::translate(glm::mat4(1.0f), center);
    model = glm::scale(model, glm::vec3(radius * 2.0f));

    glUniformMatrix4fv(
        glGetUniformLocation(shaderProgram,"model"),
        1, GL_FALSE, &model[0][0]);

    glUniformMatrix4fv(
        glGetUniformLocation(shaderProgram,"view"),
        1, GL_FALSE, &view[0][0]);

    glUniformMatrix4fv(
        glGetUniformLocation(shaderProgram,"projection"),
        1, GL_FALSE, &proj[0][0]);

    // lighter color so you can see through
    glUniform3f(
        glGetUniformLocation(shaderProgram,"color"),
        color.r * 0.5f,
        color.g * 0.5f,
        color.b * 0.5f);

    glBindVertexArray(cubeVAO);
    glDrawElements(GL_LINES, 36, GL_UNSIGNED_INT, 0);

    glDisable(GL_BLEND);
    glBindVertexArray(0);
    glUseProgram(0);
}

// todo understand why this goes here nov 6 2025
extern GLuint groundTex;  // use the one loaded in main.cpp

// void Renderer::drawGround(const glm::mat4& view, const glm::mat4& proj) {
//     glUseProgram(shaderProgram);
//     glm::mat4 model(1.0f);
//     glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"model"),1,GL_FALSE,&model[0][0]);
//     glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"view"),1,GL_FALSE,&view[0][0]);
//     glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"projection"),1,GL_FALSE,&proj[0][0]);

//     glUniform1i(glGetUniformLocation(shaderProgram,"useTex"), true);
//     glActiveTexture(GL_TEXTURE0);
//     glBindTexture(GL_TEXTURE_2D, groundTex);
//     glUniform1i(glGetUniformLocation(shaderProgram,"tex"), 0);

//     glBindVertexArray(groundVAO);
//     glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

//     glUniform1i(glGetUniformLocation(shaderProgram,"useTex"), false);
// }

void drawText2D(const char* text, float x, float y, float scale)
{
    initFont();

    static GLuint vao = 0, vbo = 0, shader = 0;
    if (!shader) {
        const char* vs = R"(
            #version 330 core
            layout (location = 0) in vec2 aPos;
            layout (location = 1) in vec2 aUV;
            out vec2 TexCoord;
            uniform vec2 screen;
            void main() {
                vec2 ndc = (aPos / screen) * 2.0 - 1.0;
                ndc.y = -ndc.y;
                gl_Position = vec4(ndc, 0.0, 1.0);
                TexCoord = aUV;
            }
        )";

        const char* fs = R"(
            #version 330 core
            in vec2 TexCoord;
            out vec4 FragColor;
            uniform sampler2D tex;
            void main() {
                float alpha = texture(tex, TexCoord).r;
                FragColor = vec4(1.0, 1.0, 1.0, alpha);
            }
        )";

        auto compile = [](GLenum type, const char* src) {
            GLuint id = glCreateShader(type);
            glShaderSource(id, 1, &src, nullptr);
            glCompileShader(id);
            return id;
        };
        GLuint vsID = compile(GL_VERTEX_SHADER, vs);
        GLuint fsID = compile(GL_FRAGMENT_SHADER, fs);
        shader = glCreateProgram();
        glAttachShader(shader, vsID);
        glAttachShader(shader, fsID);
        glLinkProgram(shader);
        glDeleteShader(vsID);
        glDeleteShader(fsID);

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
    }

    glUseProgram(shader);
    glUniform2f(glGetUniformLocation(shader, "screen"), 800.0f, 600.0f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fontTex);
    glBindVertexArray(vao);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    while (*text) {
        if (*text >= 32 && *text < 128) {
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(cdata, 512, 512, *text - 32, &x, &y, &q, 1);

            float verts[6][4] = {
                { q.x0 * scale, q.y0 * scale, q.s0, q.t0 },
                { q.x1 * scale, q.y0 * scale, q.s1, q.t0 },
                { q.x0 * scale, q.y1 * scale, q.s0, q.t1 },
                { q.x0 * scale, q.y1 * scale, q.s0, q.t1 },
                { q.x1 * scale, q.y0 * scale, q.s1, q.t0 },
                { q.x1 * scale, q.y1 * scale, q.s1, q.t1 }
            };

            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
        ++text;
    }

    glDisable(GL_BLEND);
    glBindVertexArray(0);
    glUseProgram(0);
}

void Renderer::drawGridCell(const glm::vec3& pos, const glm::mat4& view, const glm::mat4& proj) {
    glUseProgram(shaderProgram);
    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
    // z up so the small number is the last one now 
    model = glm::scale(model, glm::vec3(1.0f, 1.0f, 0.001f));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"model"),1,GL_FALSE,&model[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"view"),1,GL_FALSE,&view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"projection"),1,GL_FALSE,&proj[0][0]);
    glUniform3f(glGetUniformLocation(shaderProgram,"color"), 0.25f, 0.25f, 0.3f);
    glBindVertexArray(groundVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void Renderer::endFrame() {
    glfwSwapBuffers(window);
    glfwPollEvents();
}

bool Renderer::shouldClose() { return glfwWindowShouldClose(window); }
void Renderer::shutdown() { glfwTerminate(); }

GLuint Renderer::getShaderProgram() {
    return shaderProgram;
}
