#pragma once

#include <string>

struct OutroConfig {
    bool enabled = true;
    std::string outroPath = "assets/video/mimitaoutrov1.webm";
};

extern OutroConfig gOutroConfig;

void appendOutroToFinishedMp4(const char* replayMp4Path);
void pollOutroConfig();
void registerOutroCommands();
