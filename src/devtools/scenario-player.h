#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "input/input-frame.h"

class Player;
class Camera;

// Input-only scenario playback. Feeds the recorded fixed-tick InputFrame into
// the normal local simulation and authoritative input path, and restores the
// recorded start pose/equipment before frame 0.
class ScenarioPlayer {
public:
    struct StartState {
        glm::vec3 position{0.0f};
        glm::vec3 velocity{0.0f};
        glm::vec3 camForward{0.0f, 1.0f, 0.0f};
        float yaw = -90.0f;
        float pitch = 0.0f;
        bool grounded = false;
        int health = 100;
        std::string weapon;
        int slot = 0;
    };

    struct Tick {
        InputFrame input;
        std::string equippedWeapon;
        int equippedSlot = 0;
        glm::vec3 position{0.0f};
        glm::vec3 velocity{0.0f};
        int health = 100;
        bool grounded = false;
    };

    static ScenarioPlayer& instance();

    bool load(const std::string& path, std::string* error);
    bool begin();
    void stop(const char* reason);

    bool active() const { return mActive; }
    bool waitingForStart() const { return mWaitingForStart; }
    const std::string& path() const { return mPath; }
    const std::string& map() const { return mMap; }
    const std::string& mode() const { return mMode; }
    int weaponSet() const { return mWeaponSet; }
    const StartState& startState() const { return mStart; }
    uint32_t frameCount() const { return static_cast<uint32_t>(mTicks.size()); }
    uint32_t frame() const { return mIndex; }

    const InputFrame& currentFrame();
    void advance(Player& player, Camera& camera);

private:
    ScenarioPlayer() = default;

    bool loadLines(const std::string& path, std::string* error);
    void applyRecordedEquipment(const Tick& tick);
    void finish(const char* reason);

    bool mActive = false;
    bool mWaitingForStart = false;
    uint64_t mStartMs = 0;
    uint32_t mIndex = 0;
    float mMaxPositionError = 0.0f;
    uint32_t mFirstDivergenceFrame = 0xFFFFFFFFu;
    float mFirstDivergenceError = 0.0f;
    glm::vec3 mFirstDivergenceExpected{0.0f};
    glm::vec3 mFirstDivergenceActual{0.0f};
    uint32_t mSnapCount = 0;
    int mSavedMaxFrames = 0;
    std::string mPath;
    std::string mMap;
    std::string mMode;
    int mWeaponSet = 0;
    StartState mStart;
    std::vector<Tick> mTicks;
    InputFrame mNeutral;
    InputFrame mCurrent;
    std::string mLastAppliedWeapon;
    int mLastAppliedSlot = -1;
};
