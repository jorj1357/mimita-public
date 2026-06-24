#pragma once

#include <cstdint>
#include <string>
#include <vector>

bool decodeAudioToPCM(const std::string& path, std::vector<int16_t>& outPCM,
                      uint32_t& outSampleRate, uint32_t& outChannels,
                      uint32_t targetSampleRate = 48000, uint32_t targetChannels = 2);

std::string resolveSoundPath(const std::string& name);
