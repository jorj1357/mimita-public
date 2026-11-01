#include "audio/audio-commands.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "audio/audio.h"
#include "devtools/terminal.h"

namespace {

std::string lowercase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });
    return value;
}

} // namespace

void registerAudioCommands()
{
    Terminal::instance().registerCommand({
        "audio_probe",
        "Play a world sound at a chosen distance/direction",
        "audio_probe <sound> [distance] [front|left|right|rear]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(
                    "[AUDIO] usage: audio_probe <sound> [distance] [front|left|right|rear]");
                return;
            }

            const std::string& sound = args[0];
            float distance = 50.0f;
            if (args.size() >= 2) {
                try {
                    distance = std::max(0.0f, std::stof(args[1]));
                } catch (...) {
                    Terminal::instance().addLog("[AUDIO] invalid distance");
                    return;
                }
            }

            const std::string direction = args.size() >= 3
                ? lowercase(args[2]) : "front";
            glm::vec3 forward = audioListenerForward();
            if (glm::dot(forward, forward) < 0.0001f)
                forward = glm::vec3(0.0f, 1.0f, 0.0f);
            forward = glm::normalize(forward);

            glm::vec3 right = glm::cross(forward, glm::vec3(0.0f, 0.0f, 1.0f));
            if (glm::dot(right, right) < 0.0001f)
                right = glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f));
            if (glm::dot(right, right) < 0.0001f)
                right = glm::vec3(1.0f, 0.0f, 0.0f);
            right = glm::normalize(right);

            glm::vec3 offset = forward;
            if (direction == "left")
                offset = -right;
            else if (direction == "right")
                offset = right;
            else if (direction == "rear" || direction == "back")
                offset = -forward;

            const glm::vec3 position = audioListenerPosition() + offset * distance;
            playWorldSound(sound, position, 1.0f, 1.0f, distance);

            char message[256] = {};
            std::snprintf(message, sizeof(message),
                "[AUDIO] probe sound=%s distance=%.1f direction=%s",
                sound.c_str(), distance, direction.c_str());
            Terminal::instance().addLog(message);
        },
        "2026-10-31", CommandCategory::Debug
    });
}
