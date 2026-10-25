#pragma once

#include <cstdint>
#include <fstream>
#include <string>

class Player;
class Camera;
struct InputFrame;

// Lightweight per-tick input recorder for scenario playback. This is not the
// full replay system: it writes only input intent plus enough context to detect
// divergence, at the fixed gameplay tick rate.
class ScenarioRecorder {
public:
    static ScenarioRecorder& instance();

    bool start(const std::string& name,
               const std::string& map,
               const std::string& mode,
               int weaponSet,
               const Player& player,
               const Camera& camera,
               std::string* error = nullptr);
    void stop(const char* reason);

    bool active() const { return mActive; }
    const std::string& path() const { return mPath; }
    uint32_t frames() const { return mFrame; }

    void captureTick(const InputFrame& frame,
                     const Player& player,
                     const Camera& camera);

private:
    ScenarioRecorder() = default;

    void writeHeader(const Player& player, const Camera& camera);
    void writeFooter(const char* reason);
    void flushIfNeeded();

    bool mActive = false;
    bool mStartPending = false;
    std::ofstream mOut;
    std::string mPath;
    std::string mMap;
    std::string mMode;
    int mWeaponSet = 0;
    uint32_t mFrame = 0;
    uint64_t mStartMs = 0;
};
