// C:\important\quiet\n\mimita-private-v7-a9358d95ca5050b5d062287dd41ee2e63709f9a5\src\gui\font-stuff\font-loader.cpp
// mar 13 2026
/**
 * purpose
 * Where the .fnt loading goes

Create a separate file:

src/gui/font-stuff/font-loader.cpp

Example functions:

bool loadFontAtlas()
bool loadFontGlyphs()

Call them once at startup.
 */

#include "font-glyph.h"
#include "font-loader.h"
#include <cstdio>
#include <fstream>
#include <string>
#include <glad/glad.h>

// do not define stb image implementation here 
// it only goes in C:\important\quiet\n\mimita-private-v7-a9358d95ca5050b5d062287dd41ee2e63709f9a5\src\utils\stb_image_impl.cpp
#include "stb_image.h"

// here so that we can print dir mar 14 2026

Glyph gGlyphs[256];

GLuint gFontTex = 0;

// font-loader.cpp (the only definition) of guivao and guivbo
GLuint guiVAO = 0;
GLuint guiVBO = 0;

int atlasWidth = 0;
int atlasHeight = 0;

void fontInit()
{
    printf("[FONT] init\n");

    // mar 14 2026 we have 2 of these, not just 1 
    // its a _0 and a _1 
    loadFontAtlas("assets/font/mingliu-mimita-v3_0.png");
    // loadFontGlyphs("assets/font/mingliu-mimita-v3.fnt");
    // mar 14 2026 testing absolute path loading
    loadFontGlyphs("C:/important/quiet/n/mimita-private-v7-a9358d95ca5050b5d062287dd41ee2e63709f9a5/assets/font/mingliu-mimita-v3.fnt");

    // we must have this stuff below 
    // or else it just draws nothing
    glGenVertexArrays(1,&guiVAO);
    glGenBuffers(1,&guiVBO);

    // mar 14 2026 this is in font-loader (this file) and gui-label
    // not good? 
    glBindVertexArray(guiVAO);
    glBindBuffer(GL_ARRAY_BUFFER,guiVBO);

    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(float)*30,
        nullptr,
        GL_DYNAMIC_DRAW
    );

    glVertexAttribPointer(
        0,3,GL_FLOAT,GL_FALSE,
        5*sizeof(float),(void*)0
    );
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1,2,GL_FLOAT,GL_FALSE,
        5*sizeof(float),(void*)(3*sizeof(float))
    );
    glEnableVertexAttribArray(1);

    printf("[FONT] VAO/VBO ready\n");
}
bool loadFontAtlas(const char* path)
{
    int channels;

    unsigned char* data =
        stbi_load(path,&atlasWidth,&atlasHeight,&channels,4);

    if(!data)
    {
        printf("[FONT] atlas load failed\n");
        return false;
    }

    glGenTextures(1,&gFontTex);
    glBindTexture(GL_TEXTURE_2D,gFontTex);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        atlasWidth,
        atlasHeight,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        data
    );

    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

    stbi_image_free(data);

    printf("[FONT] atlas loaded %dx%d\n",atlasWidth,atlasHeight);
    printf("[FONT] about to reading glyphs...\n");

    return true;
}

// testing new version mar 14 2026 with no std::ifstream
bool loadFontGlyphs(const char* path)
{
    printf("[FONT] reading glyphs...\n");
    printf("[FONT] opening file: %s\n", path);

    FILE* f = fopen(path, "rb");
    if (!f)
    {
        printf("[FONT] FAILED TO OPEN FILE\n");
        return false;
    }

    char line[1024];

    while (fgets(line, sizeof(line), f))
    {
        int id;
        Glyph g;

        if (sscanf(
                line,
                "char id=%d x=%d y=%d width=%d height=%d xoffset=%d yoffset=%d xadvance=%d",
                &id, &g.x, &g.y, &g.w, &g.h, &g.xoffset, &g.yoffset, &g.xadvance
            ) == 8)
        {
            if (id >= 0 && id < 256)
                gGlyphs[id] = g;
        }
    }

    fclose(f);

    printf("[FONT] glyphs loaded\n");
    return true;
}

// this version breaks mar 14 2026? because of std:ifstream? idk 
// bool loadFontGlyphs(const char* path)
// {
//     printf("[FONT] reading glyphs... attempt 1\n");
//     printf("[FONT] opening file: %s\n", path);

//     std::ifstream f(path);

    
//     if(!f.is_open())
//     {
//         printf("[FONT] FAILED TO OPEN FILE is_open version\n");
//         return false;
//     }

//     if(!f)
//     {
//         printf("[FONT] failed to open fnt just f version \n");
//         return false;
//     }

//     std::string line;

//     printf("[FONT] reading glyphs... attempt 2 stdgetline \n");

//     while(std::getline(f,line))
//     {
//             if(line.find("char id=")==0)
//             {
//             Glyph g;
//             int id;

//             sscanf(
//                 line.c_str(),
//                 "char id=%d x=%d y=%d width=%d height=%d xoffset=%d yoffset=%d xadvance=%d",
//                 &id,&g.x,&g.y,&g.w,&g.h,&g.xoffset,&g.yoffset,&g.xadvance
//             );

//             if(id >= 0 && id < 256)
//             {
//                 gGlyphs[id] = g;
//             }        
//         }
//     }

//     printf("[FONT] glyphs loaded\n");

//     return true;
// }