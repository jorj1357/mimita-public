// C:\important\quiet\n\mimita-private-v7-a9358d95ca5050b5d062287dd41ee2e63709f9a5\src\gui\font-stuff\font-glyph.h
// mar 13 2026
/**
 * purpose
 * define glyph so that we dont load a .ttf 
 * bc that breaks things?
 * just read from the png and use that as the font
 */

#pragma once

struct Glyph
{
    int id = 0;
    int x;
    int y;
    int w;
    int h;

    int xoffset;
    int yoffset;
    int xadvance;
    int page = 0;
};
