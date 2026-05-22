// C:\important\quiet\n\mimita-priv-v7\src\game\game-state.h
// mar 8 2026
/**
 * purpose
 * sets if we are in menu
 * or 
 * pause menu thats ingame
 * or
 * in a diffrent gui
 * or
 * etc etc
 * like are we talking to an npc or do we have the rspawn screen on the screen etc
 */

#pragma once

enum GameState
{
    GAME_MENU,
    GAME_PLAYING,
    GAME_PAUSED
};