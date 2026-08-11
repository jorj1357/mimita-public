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
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <unordered_map>
#include <glad/glad.h>
#include "debug/gl-debug.h"

// do not define stb image implementation here 
// it only goes in C:\important\quiet\n\mimita-private-v7-a9358d95ca5050b5d062287dd41ee2e63709f9a5\src\utils\stb_image_impl.cpp
#include "stb_image.h"

// here so that we can print dir mar 14 2026

Glyph gGlyphs[256];

GLuint gFontTex = 0;
GLuint gFontPages[8] = {};
int gFontPageCount = 0;

// font-loader.cpp (the only definition) of guivao and guivbo
GLuint guiVAO = 0;
GLuint guiVBO = 0;

int atlasWidth = 0;
int atlasHeight = 0;
int fontLineHeight = 48;
int fontBase = 38;

static bool gFontReady = false;
static std::unordered_map<unsigned int, Glyph> gGlyphMap;
static std::unordered_map<unsigned long long, int> gKerning;

static int readIntField(const char* line, const char* key, int fallback = 0)
{
    const char* p = strstr(line, key);
    if (!p) return fallback;
    p += strlen(key);
    return atoi(p);
}

static bool readQuotedField(const char* line, const char* key, char* out, size_t outSize)
{
    const char* p = strstr(line, key);
    if (!p) return false;
    p += strlen(key);
    const char* end = strchr(p, '"');
    if (!end) return false;
    size_t n = (size_t)(end - p);
    if (n >= outSize) n = outSize - 1;
    memcpy(out, p, n);
    out[n] = 0;
    return true;
}

static GLuint loadFontAtlasPage(const char* path, int pageId)
{
    printf("[FONT] Loaded atlas page request id=%d path=%s\n", pageId, path);
    int channels = 0;
    int w = 0;
    int h = 0;
    unsigned char* data = stbi_load(path, &w, &h, &channels, 4);
    if (!data || w <= 0 || h <= 0)
    {
        printf("[FONT ERROR] Failed to load atlas page %d: %s\n", pageId, path);
        if (data)
            stbi_image_free(data);
        return 0;
    }

    GLuint tex = 0;
    MIMITA_GL_CLEAR_STAGE("loadFontAtlasPage");
    MIMITA_GL_CALL(glGenTextures(1, &tex));
    if (!tex)
    {
        stbi_image_free(data);
        return 0;
    }
    MIMITA_GL_CALL(glBindTexture(GL_TEXTURE_2D, tex));
    // 5 23 2026 fonts not wokring? added this
    MIMITA_GL_CALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    MIMITA_GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data));
    MIMITA_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    MIMITA_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    MIMITA_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    MIMITA_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    MIMITA_GL_CALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 4));
    stbi_image_free(data);

    if (pageId >= 0 && pageId < 8)
    {
        gFontPages[pageId] = tex;
        if (pageId + 1 > gFontPageCount)
            gFontPageCount = pageId + 1;
    }
    if (pageId == 0)
        gFontTex = tex;

    atlasWidth = w;
    atlasHeight = h;
    printf("[FONT] Loaded atlas page id=%d tex=%u size=%dx%d\n", pageId, tex, w, h);
    return tex;
}

void fontInit()
{
    printf("[FONT] Loading bitmap font\n");

    bool glyphsOk = loadFontGlyphs("assets/font/noto-serif-cjk-tc-mimita-v1.fnt");
    bool atlasOk = gFontPageCount > 0 && gFontPages[0] != 0;
    if (!atlasOk || !glyphsOk)
        printf("[FONT WARNING] asset font failed; UI will use visible fallback text\n");
    gFontReady = atlasOk && glyphsOk;

    // we must have this stuff below 
    // or else it just draws nothing
    MIMITA_GL_CLEAR_STAGE("fontInit");
    MIMITA_GL_CALL(glGenVertexArrays(1,&guiVAO));
    MIMITA_GL_CALL(glGenBuffers(1,&guiVBO));

    // mar 14 2026 this is in font-loader (this file) and gui-label
    // not good? 
    MIMITA_GL_CALL(glBindVertexArray(guiVAO));
    MIMITA_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER,guiVBO));

    MIMITA_GL_CALL(glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(float)*30,
        nullptr,
        GL_DYNAMIC_DRAW
    ));

    MIMITA_GL_CALL(glVertexAttribPointer(
        0,3,GL_FLOAT,GL_FALSE,
        5*sizeof(float),(void*)0
    ));
    MIMITA_GL_CALL(glEnableVertexAttribArray(0));

    MIMITA_GL_CALL(glVertexAttribPointer(
        1,2,GL_FLOAT,GL_FALSE,
        5*sizeof(float),(void*)(3*sizeof(float))
    ));
    MIMITA_GL_CALL(glEnableVertexAttribArray(1));

    printf("[FONT] VAO/VBO ready\n");
}
bool loadFontAtlas(const char* path)
{
    return loadFontAtlasPage(path, 0) != 0;
}

// testing new version mar 14 2026 with no std::ifstream
bool loadFontGlyphs(const char* path)
{
    printf("[FONT] Loading bitmap font descriptor\n");
    printf("[FONT] opening file: %s\n", path);

    FILE* f = fopen(path, "rb");
    if (!f)
    {
        printf("[FONT ERROR] Failed to parse .fnt: could not open file\n");
        return false;
    }

    gGlyphMap.clear();
    gKerning.clear();
    for (Glyph& glyph : gGlyphs)
        glyph = Glyph{};
    memset(gFontPages, 0, sizeof(gFontPages));
    gFontPageCount = 0;

    char line[1024];

    while (fgets(line, sizeof(line), f))
    {
        if (strncmp(line, "common ", 7) == 0)
        {
            fontLineHeight = readIntField(line, "lineHeight=", fontLineHeight);
            fontBase = readIntField(line, "base=", fontBase);
            atlasWidth = readIntField(line, "scaleW=", atlasWidth);
            atlasHeight = readIntField(line, "scaleH=", atlasHeight);
            printf("[FONT] common lineHeight=%d base=%d atlas=%dx%d\n", fontLineHeight, fontBase, atlasWidth, atlasHeight);
        }
        else if (strncmp(line, "page ", 5) == 0)
        {
            int id = readIntField(line, "id=", 0);
            char fileName[256];
            if (readQuotedField(line, "file=\"", fileName, sizeof(fileName)))
            {
                char atlasPath[512];
                snprintf(atlasPath, sizeof(atlasPath), "assets/font/%s", fileName);
                loadFontAtlasPage(atlasPath, id);
            }
        }
        else if (strncmp(line, "char ", 5) == 0)
        {
            Glyph g{};
            g.id = readIntField(line, "id=", 0);
            g.x = readIntField(line, "x=", 0);
            g.y = readIntField(line, "y=", 0);
            g.w = readIntField(line, "width=", 0);
            g.h = readIntField(line, "height=", 0);
            g.xoffset = readIntField(line, "xoffset=", 0);
            g.yoffset = readIntField(line, "yoffset=", 0);
            g.xadvance = readIntField(line, "xadvance=", 0);
            g.page = readIntField(line, "page=", 0);
            gGlyphMap[(unsigned int)g.id] = g;
            if (g.id >= 0 && g.id < 256)
                gGlyphs[g.id] = g;
        }
        else if (strncmp(line, "kerning ", 8) == 0)
        {
            unsigned int first = (unsigned int)readIntField(line, "first=", 0);
            unsigned int second = (unsigned int)readIntField(line, "second=", 0);
            int amount = readIntField(line, "amount=", 0);
            unsigned long long key = ((unsigned long long)first << 32) | second;
            gKerning[key] = amount;
        }
    }

    fclose(f);

    printf("[FONT] Loaded glyphs count=%zu kernings=%zu pages=%d\n", gGlyphMap.size(), gKerning.size(), gFontPageCount);
    return true;
}

bool fontReady()
{
    return gFontReady;
}

bool fontGetGlyph(unsigned int codepoint, Glyph& out)
{
    auto it = gGlyphMap.find(codepoint);
    if (it == gGlyphMap.end())
    {
        static int missingPrints = 0;
        if (missingPrints < 20)
        {
            printf("[FONT] Missing glyph codepoint=%u\n", codepoint);
            missingPrints++;
        }
        return false;
    }
    out = it->second;
    return true;
}

int fontGetKerning(unsigned int first, unsigned int second)
{
    unsigned long long key = ((unsigned long long)first << 32) | second;
    auto it = gKerning.find(key);
    if (it == gKerning.end())
        return 0;
    return it->second;
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
