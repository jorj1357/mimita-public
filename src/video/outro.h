#pragma once

#include <string>

struct OutroConfig {
    bool enabled = true;
    std::string outroPath = "assets/video/mimitaoutrov1.webm";
};

extern OutroConfig gOutroConfig;

// Appends the outro to the finished replay MP4 in a single ffmpeg pass.
// replayW/replayH come from the export job; when 0 (manual/test commands) the
// resolution is probed via `ffmpeg -i` output (no ffprobe is shipped).
void appendOutroToFinishedMp4(const char* replayMp4Path, int replayW, int replayH, bool hasAudio);

// ffprobe-free video size probe used by manual outro commands.
bool probeVideoSizeViaFfmpeg(const std::string& path, int& outW, int& outH);
void pollOutroConfig();
void registerOutroCommands();
