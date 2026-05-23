// C:\important\quiet\n\mimita-private-v7-a9358d95ca5050b5d062287dd41ee2e63709f9a5\src\gui\font-stuff\font-loader.h
// mar 13 2026
/** purpose
 * 
 * This header tells the rest of the program:

these variables and functions exist somewhere else
 */

#pragma once

#include "font-glyph.h"
#include <glad/glad.h>

extern Glyph gGlyphs[256];

extern GLuint gFontTex;
extern GLuint gFontPages[8];
extern int gFontPageCount;

extern GLuint guiVAO;
extern GLuint guiVBO;

extern int atlasWidth;
extern int atlasHeight;
extern int fontLineHeight;
extern int fontBase;

bool loadFontAtlas(const char* path);
bool loadFontGlyphs(const char* path);
void fontInit();
bool fontReady();
bool fontGetGlyph(unsigned int codepoint, Glyph& out);
int fontGetKerning(unsigned int first, unsigned int second);
