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
    // How the raw axis is interpreted. "auto" keeps the legacy behaviour
    // (signed stick or trigger normalised from -1..1); "axis01" treats the
    // value as already 0..1; "combined" reads one half of a shared trigger
    // axis; "signed" forces the -1..1 interpretation.
    std::string mode = "auto";
    // Sign for "combined" mode: -1 selects the negative half, +1 the positive.
    float sign = 1.0f;
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
    const std::string& captureMode() const { return mCaptureMode; }

    // Focused-window policy: mode "focused" (default) reads the pad only while
    // this process's game window has OS focus; mode "always" reads it whenever
    // the process is open. The old behaviour was "always".
    void setWindowFocused(bool focused) { mWindowFocused = focused; }

    // Bounded diagnostic override used to prove the controller -> vehicle path
    // when no physical pad is available. It is not a second gameplay path.
    void setVirtualState(float throttle, float brake, float steer, bool jump,
                         bool boost, bool powerslide, bool ballcam);
    void clearVirtualState();
    bool virtualActive() const { return mVirtual; }

    // Emit one structured snapshot of the current device and logical actions.
    void logState(const char* reason);

    bool held(const std::string& action) const;
    // Raw device axis value (index 0..15), for diagnostics only.
    float rawAxis(int axis) const
    {
        return (axis >= 0 && axis < mAxisCount && axis < 16) ? mAxes[axis]
                                                             : 0.0f;
    }
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
    void logAxes();
    void logPoll();
    void updateTriggerCalibration();
    void finishTriggerCalibration();
    float axisValue(int axis, bool trigger, bool invert, float deadzone,
                    float upperDeadzone = 1.0f,
                    const std::string& mode = "auto",
                    float sign = 1.0f) const;
    bool buttonValue(int button) const;

    std::unordered_map<std::string, GamepadBinding> mBindings;
    bool mConnected = false;
    bool mStandardMapping = false;
    bool mRawOnly = false;
    int mDeviceId = -1;
    // Trigger calibration. A pad's trigger axes rest at -1 (signed), 0 (0..1)
    // or +1 (inverted); guessing wrong makes a released trigger read as half
    // pressed, which permanently fights the car's drive. The rest value and the
    // observed travel are measured per axis when a device connects.
    struct AxisCalibration
    {
        float rest = 0.0f;
        float lo = 0.0f;
        float hi = 1.0f;
        bool valid = false;
    };
    AxisCalibration mAxisCal[16];
    int mCalibratedDeviceId = -1;
    int mCalibrationFrames = 0;
    std::string mName;
    std::string mConfigPath = "config/input/gamepad.json";
    std::string mMappingsPath = "config/input/gamecontrollerdb.txt";
    std::string mCaptureMode = "focused";
    bool mWindowFocused = false;

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
