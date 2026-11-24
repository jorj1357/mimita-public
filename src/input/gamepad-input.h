// 2026-11-24
/* purpose
* One owner for physical gamepad/joystick sampling.
* Uses GLFW's standardized gamepad mapping when the device has one, and a raw
* axis/button fallback for unmapped devices. Produces logical action values for
* the existing InputCommandSystem/vehicle input owners.
* Does NOT run gameplay physics, send packets, or own keyboard/mouse input.
*/
#pragma once

#include <string>
#include <unordered_map>

struct GamepadBinding {
    int button = -1;
    int axis = -1;
    bool trigger = false;
    bool invert = false;
    float deadzone = 0.15f;
    // Value at which the axis reports full travel. < 1.0 makes a trigger reach
    // 100% before it is physically fully pressed, with a linear ramp below.
    float upperDeadzone = 1.0f;
};

class GamepadInput {
public:
    static GamepadInput& instance();

    // Poll once per frame before input actions are consumed.
    void poll();
    bool reloadConfig();

    bool connected() const { return mConnected; }
    const std::string& name() const { return mName; }
    int deviceId() const { return mDeviceId; }
    bool standardMapping() const { return mStandardMapping; }
    bool rawFallback() const { return mRawOnly; }

    // Bounded diagnostic override used to prove the controller -> vehicle path
    // when no physical pad is available. It is not a second gameplay path.
    void setVirtualState(float throttle, float brake, float steer, bool jump,
                         bool boost, bool powerslide, bool ballcam);
    void clearVirtualState();
    bool virtualActive() const { return mVirtual; }

    // Emit one structured snapshot of the current device and logical actions.
    void logState(const char* reason);

    bool held(const std::string& action) const;
    float value(const std::string& action) const;

    // Logical vehicle helpers. Values are normalized 0..1 (throttle/brake) or
    // -1..1 (steer).
    float throttle() const { return value("throttle"); }
    float brake() const { return value("brake"); }
    float steer() const { return value("steer"); }
    float pitch() const { return value("pitch"); }
    float swivelX() const { return value("swivelX"); }
    float swivelY() const { return value("swivelY"); }
    bool boost() const { return held("boost"); }
    bool jump() const { return held("jump"); }
    bool powerslide() const { return held("powerslide"); }
    bool ballCam() const { return held("ballcam"); }
    bool airRollLeft() const { return held("airRollLeft"); }
    bool airRollRight() const { return held("airRollRight"); }
    bool lookBehind() const { return held("lookBehind"); }

private:
    GamepadInput();

    void applyDefaultBindings();
    void loadMappings();
    void pollState();
    float axisValue(int axis, bool trigger, bool invert, float deadzone,
                    float upperDeadzone = 1.0f) const;
    bool buttonValue(int button) const;

    std::unordered_map<std::string, GamepadBinding> mBindings;
    bool mConnected = false;
    bool mStandardMapping = false;
    bool mRawOnly = false;
    int mDeviceId = -1;
    std::string mName;
    std::string mConfigPath = "config/input/gamepad.json";
    std::string mMappingsPath = "config/input/gamecontrollerdb.txt";

    // GLFW standardized state, or raw fallback.
    float mAxes[16] = {};
    unsigned char mButtons[32] = {};
    int mAxisCount = 0;
    int mButtonCount = 0;

    bool mLoggedConnectionState = false;
    bool mVirtual = false;
    float mVirtualThrottle = 0.0f;
    float mVirtualBrake = 0.0f;
    float mVirtualSteer = 0.0f;
    bool mVirtualJump = false;
    bool mVirtualBoost = false;
    bool mVirtualPowerslide = false;
    bool mVirtualBallcam = false;
};
