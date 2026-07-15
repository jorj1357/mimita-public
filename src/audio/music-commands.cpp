#include <cstdio>
#include <string>
#include <vector>
#include "devtools/terminal.h"
#include "audio/music-manager.h"
#include "config/player-settings.h"
#include "debug/debug-log.h"

void registerMusicCommands()
{
    Terminal::instance().registerCommand({
        "music_next", "Skip to next music track", "music_next",
        [](const std::vector<std::string>&) { MusicManager::instance().skip(); },
        "2026-06-14", CommandCategory::Debug
    });
    Terminal::instance().registerCommand({
        "music_prev", "Go to previous music track", "music_prev",
        [](const std::vector<std::string>&) { MusicManager::instance().previous(); },
        "2026-06-14", CommandCategory::Debug
    });
    Terminal::instance().registerCommand({
        "music_pause", "Pause music playback", "music_pause",
        [](const std::vector<std::string>&) { MusicManager::instance().pause(); },
        "2026-06-14", CommandCategory::Debug
    });
    Terminal::instance().registerCommand({
        "music_resume", "Resume music playback", "music_resume",
        [](const std::vector<std::string>&) { MusicManager::instance().resume(); },
        "2026-06-14", CommandCategory::Debug
    });
    Terminal::instance().registerCommand({
        "music_stop", "Stop music playback", "music_stop",
        [](const std::vector<std::string>&) { MusicManager::instance().stop(); },
        "2026-06-14", CommandCategory::Debug
    });
    Terminal::instance().registerCommand({
        "music_reload", "Reload music files and credits", "music_reload",
        [](const std::vector<std::string>&) { MusicManager::instance().reload(); },
        "2026-06-14", CommandCategory::Debug
    });
    Terminal::instance().registerCommand({
        "music_info", "Show current track info", "music_info",
        [](const std::vector<std::string>&) {
            Terminal::instance().addLog("[MUSIC] " + MusicManager::instance().currentTrackInfo());
        },
        "2026-06-14", CommandCategory::Debug
    });
    Terminal::instance().registerCommand({
        "music_debug_ui", "Toggle music widget debug overlay", "music_debug_ui [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                MusicManager::instance().setWidgetDebug(!MusicManager::instance().widgetDebug());
            } else {
                MusicManager::instance().setWidgetDebug(args[0] == "1");
            }
            const bool on = MusicManager::instance().widgetDebug();
            Terminal::instance().addLog(on
                ? "[MUSIC] Widget debug ON"
                : "[MUSIC] Widget debug OFF");
        },
        "2026-06-14", CommandCategory::Debug
    });
    Terminal::instance().registerCommand({
        "music_volume", "Set music volume (0.0 - 1.0)", "music_volume <volume>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                char buf[64];
                snprintf(buf, sizeof(buf), "[MUSIC] volume=%.2f", MusicManager::instance().volume());
                Terminal::instance().addLog(buf);
                return;
            }
            float vol = std::clamp(std::stof(args[0]), 0.0f, 1.0f);
            MusicManager::instance().setVolume(vol);
            GetPlayerSettings().musicVolume = vol;
            SavePlayerSettings();
        },
        "2026-06-14", CommandCategory::Debug
    });
    Terminal::instance().registerCommand({
        "music_mute", "Toggle music mute on/off", "music_mute [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                bool muted = MusicManager::instance().muted();
                Terminal::instance().addLog(muted
                    ? "[MUSIC] muted"
                    : "[MUSIC] unmuted");
                return;
            }
            bool mute = args[0] != "0";
            MusicManager::instance().setMuted(mute);
            GetPlayerSettings().musicMuted = mute;
            SavePlayerSettings();
            Terminal::instance().addLog(mute
                ? "[MUSIC] muted"
                : "[MUSIC] unmuted");
        },
        "2026-06-14", CommandCategory::Debug
    });
    Terminal::instance().registerCommand({
        "music_debug", "Show music config state", "music_debug",
        [](const std::vector<std::string>&) {
            auto& mm = MusicManager::instance();
            char buf[256];
            snprintf(buf, sizeof(buf),
                "musicEnabled=%d\nmusicVolume=%.2f\nmusicSpeed=%.2f\nconfigLoaded=1\nconfigPath=config/audio/music-settings.json",
                (int)!mm.muted(), mm.volume(), mm.playbackSpeed());
            Terminal::instance().addLog(buf);
            Debug::log(Debug::Category::Audio, "[MUSIC] debug: enabled=%d volume=%.2f speed=%.2f\n",
                       (int)!mm.muted(), mm.volume(), mm.playbackSpeed());
        },
        "2026-06-14", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "music_speed", "Set music playback speed (0.25-2.0)", "music_speed <value>",
        [](const std::vector<std::string>& args) {
            auto& mm = MusicManager::instance();
            if (args.empty()) {
                Terminal::instance().addLog(std::string("[MUSIC] speed=") + std::to_string(mm.playbackSpeed()) + "x");
                return;
            }
            float speed = std::stof(args[0]);
            mm.setPlaybackSpeed(speed);
            Terminal::instance().addLog(std::string("[MUSIC] speed set to ") + std::to_string(mm.playbackSpeed()) + "x");
        }
    });

    Terminal::instance().registerCommand({
        "music_speed_up", "Increase music playback speed by 0.1", "music_speed_up",
        [](const std::vector<std::string>&) {
            auto& mm = MusicManager::instance();
            mm.setPlaybackSpeed(mm.playbackSpeed() + 0.1f);
            Terminal::instance().addLog(std::string("[MUSIC] speed=") + std::to_string(mm.playbackSpeed()) + "x");
        }
    });

    Terminal::instance().registerCommand({
        "music_speed_down", "Decrease music playback speed by 0.1", "music_speed_down",
        [](const std::vector<std::string>&) {
            auto& mm = MusicManager::instance();
            mm.setPlaybackSpeed(mm.playbackSpeed() - 0.1f);
            Terminal::instance().addLog(std::string("[MUSIC] speed=") + std::to_string(mm.playbackSpeed()) + "x");
        }
    });

    Terminal::instance().registerCommand({
        "music_speed_reset", "Reset music playback speed to 1.0", "music_speed_reset",
        [](const std::vector<std::string>&) {
            MusicManager::instance().setPlaybackSpeed(1.0f);
            Terminal::instance().addLog("[MUSIC] speed reset to 1.00x");
        }
    });

    Terminal::instance().registerCommand({
        "volume", "Set master volume (0.0 = mute, 1.0 = full)", "volume <0.0-1.0>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                char buf[64];
                snprintf(buf, sizeof(buf), "[AUDIO] Volume: %.2f", GetPlayerSettings().masterVolume);
                Terminal::instance().addLog(buf);
                Terminal::instance().addLog("[AUDIO] Usage: volume <0.0-1.0> (0=mute, 1=full)");
                return;
            }
            float vol = std::clamp(std::stof(args[0]), 0.0f, 1.0f);
            GetPlayerSettings().masterVolume = vol;
            GetPlayerSettings().musicVolume = vol;
            MusicManager::instance().setVolume(vol);
            SavePlayerSettings();
            char buf[64];
            snprintf(buf, sizeof(buf), "[AUDIO] Volume set to %.2f", vol);
            Terminal::instance().addLog(buf);
        }
    });

    Terminal::instance().registerCommand({
        "music_status", "Show current music state", "music_status",
        [](const std::vector<std::string>&) {
            auto& mm = MusicManager::instance();
            char buf[256];
            snprintf(buf, sizeof(buf),
                "Volume: %.2f\nMuted: %d\nSpeed: %.2fx\nPlaying: %d\nTrack: %s",
                mm.volume(), (int)mm.muted(), mm.playbackSpeed(),
                (int)mm.isPlaying(), mm.currentTrackInfo().c_str());
            Terminal::instance().addLog(buf);
        }
    });
}
