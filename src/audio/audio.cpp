// C:\important\quiet\n\mimita-priv-v7\src\audio\audio.cpp
/**
 * purpose
 * functions so we plau adio 
 * and it sound good
 * i think feb 10 2026 was date idk 
 * i wrote dat mar 7 2026
 * also its messy with comments about caht gpt 
 * and the sound pool and sound queue and stuff
 * but i think right now i idont reallu even care
 * i like the fucked up broken bullshti version of it 
 * it gives it flavor 
 * no polished triple AAA game studio here i hate the polished nice clean
 * i want dirty real raw
 */

// audio.cpp
// THIS IS THE ONL PLACE THAT MINIAUDIO_IMPLEMENTATION CAN BE DEFINED 

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "audio.h"
#include <string>
#include <cstdio>
#include <cstdlib>

static ma_engine gEngine;
static bool gAudioInit = false;

// cooldown timer for air jump
static float airJumpCooldown = 0.0f;

static void initAudioOnce()
{
    if (gAudioInit) return;

    if (ma_engine_init(NULL, &gEngine) != MA_SUCCESS)
    {
        printf("[AUDIO] Engine failed\n");
        return;
    }

    gAudioInit = true;
    printf("[AUDIO] Engine initialized\n");
}

void audioUpdate(float dt)
{
    if (airJumpCooldown > 0.0f)
        airJumpCooldown -= dt;
}

void playSound(const std::string& name, float volume)
{
    initAudioOnce();

    std::string path = "assets/sound/" + name + ".wav";

    ma_engine_play_sound(&gEngine, path.c_str(), NULL);
}

void playAirJumpSound()
{
    if (airJumpCooldown > 0.0f)
        return;

    playSound("entity/player/doublejump", 1.0f);

    // how much time to wait beofre playing the sound again 
    airJumpCooldown = 0.5f;
}

void playRandomFootstep()
{
    int r = rand() % 4;

    switch (r)
    {
        case 0: playSound("entity/player/walk1",1.0f); break;
        case 1: playSound("entity/player/walk2",1.0f); break;
        case 2: playSound("entity/player/walk3",1.0f); break;
        case 3: playSound("entity/player/walk4",1.0f); break;
    }
}

// mar 8 2026 todo maibe put this in like its own file or own folder idk
void playFreezeBeginSound()
{
    playSound("entity/player/freezebegin", 1.0f);
}

void playFreezeHoldSound()
{
    playSound("entity/player/freezehold", 1.0f);
}

void playFreezeEndSound()
{
    playSound("entity/player/freezeend", 1.0f);
}