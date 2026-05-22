// C:\important\quiet\n\mimita-priv-v7\src\gui\gui-label.cpp
// mar 8 2026
/**
 * purpose
 * exposes A SINGLE FUNCTION
 * 
 * textLabel(args)
 * and the args are like
 * text in the button
 * size of button
 * position of button etc
 */

/**
 * mar 8 2026 todo
 * split this into like
 * another file that renders text
 * or does fonts
 * just dont 
 * dont do fonts all in this file 
 * need a specific file that exposes
 * doFont(args)
 * and all we do here
 * is just call it 
 */

/**
 * mar 14 2026
 * gui-label.cpp should only render text, not load fonts.
 */

#include "gui-label.h"
#include "gui/font-stuff/font-loader.h"

#include <cstdio>
#include <vector>
#include <fstream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

// dont incude? im confused mar 13 2026 
// static GLuint gFontTex = 0;

static const int FONT_BITMAP_W = 512;
static const int FONT_BITMAP_H = 512;


// do not put fontInit() here
//do not put guivao and guivbo either 

// mar 14 2026 commented out bc not used? idk   
// static bool loadFile(const char* path, std::vector<unsigned char>& out)
// {
//     printf("[GUI LABEL] loading file: %s\n", path);

//     std::ifstream f(path,std::ios::binary);

//     if(!f)
//     {
//         printf("[GUI LABEL] failed to open file\n");
//         return false;
//     }

//     f.seekg(0,std::ios::end);
//     size_t size = f.tellg();
//     f.seekg(0,std::ios::beg);

//     out.resize(size);
//     f.read((char*)out.data(),size);

//     printf("[GUI LABEL] loaded %zu bytes\n",size);

//     return true;
// }

void guiLabel(const char* text, float x, float y)
{
    glBindTexture(GL_TEXTURE_2D, gFontTex);
    // add these 2 things so it works? idk 
    
    // mar 14 2026 this is in font-loader and gui-label (this file)
    // not good? 
    glBindVertexArray(guiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, guiVBO);

    float cursorX = x;
    float cursorY = y;

    while (*text)
    {
        unsigned char c = *text;

        Glyph& g = gGlyphs[c];

        float x0 = cursorX + g.xoffset;
        float y0 = cursorY + g.yoffset;

        float x1 = x0 + g.w;
        float y1 = y0 + g.h;

        float u0 = (float)g.x / atlasWidth;
        float v0 = (float)g.y / atlasHeight;
        float u1 = (float)(g.x + g.w) / atlasWidth;
        float v1 = (float)(g.y + g.h) / atlasHeight;

        // float verts[] = {
        //     x0,y0,u0,v0,
        //     x1,y0,u1,v0,
        //     x1,y1,u1,v1,

        //     x0,y0,u0,v0,
        //     x1,y1,u1,v1,
        //     x0,y1,u0,v1
        // };

        // mar 13 2026 idk what to do w this or where put it 
        float verts[]={
            x0,y0,0,u0,v0,
            x1,y0,0,u1,v0,
            x1,y1,0,u1,v1,

            x0,y0,0,u0,v0,
            x1,y1,0,u1,v1,
            x0,y1,0,u0,v1
        };

        // mar 14 2026 
        // this happens per letter, per glyph etc
        // too much performance cost
        // batch later todo
        glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(verts),verts);
        glDrawArrays(GL_TRIANGLES,0,6);

        cursorX += g.xadvance;

        ++text;
    }
}