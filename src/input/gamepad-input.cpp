// 2026-11-24

#include "input/gamepad-input.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

#include <GLFW/glfw3.h>
#include <nlohmann/json.hpp>

#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "utils/json-comments.h"

using json = nlohmann::json;

namespace {

bool ensureGamepadGlfw()
{
    static int state = 0;  // 0 = unknown, 1 = ready, -1 = failed
    if (state == 0)
    {
        state = glfwInit() ? 1 : -1;
        if (state < 0)
        {
            const char* error = nullptr;
            glfwGetError(&error);
            StructuredLogger::instance().writeEvent(
                StructuredCategory::General, StructuredLevel::Errors,
                "input.gamepad.glfw-init", "gamepad",
                "gamepad GLFW initialization failed",
                0,
                {{"result", "failed"},
                 {"glfw_error", error ? error : ""}});
        }
    }
    return state > 0;
}

float applyDeadzone(float value, float deadzone, float upperDeadzone = 1.0f)
{
    const float magnitude = std::fabs(value);
    if (magnitude <= deadzone)
        return 0.0f;
    const float span = std::max(
        std::clamp(upperDeadzone, deadzone + 0.01f, 1.0f) - deadzone, 0.01f);
    const float scaled = (magnitude - deadzone) / span;
    return std::copysign(std::clamp(scaled, 0.0f, 1.0f), value);
}

}  // namespace

GamepadInput::GamepadInput()
{
    applyDefaultBindings();
}

GamepadInput& GamepadInput::instance()
{
    static GamepadInput input;
    return input;
}

void GamepadInput::applyDefaultBindings()
{
    mBindings.clear();

    GamepadBinding steer;
    steer.axis = GLFW_GAMEPAD_AXIS_LEFT_X;
    steer.deadzone = 0.15f;
    mBindings["steer"] = steer;

    GamepadBinding throttle;
    throttle.axis = GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER;
    throttle.trigger = true;
    throttle.deadzone = 0.02f;
    throttle.upperDeadzone = 0.85f;
    mBindings["throttle"] = throttle;

    GamepadBinding brake;
    brake.axis = GLFW_GAMEPAD_AXIS_LEFT_TRIGGER;
    brake.trigger = true;
    brake.deadzone = 0.02f;
    brake.upperDeadzone = 0.85f;
    mBindings["brake"] = brake;

    mBindings["jump"].button = GLFW_GAMEPAD_BUTTON_A;
    mBindings["boost"].button = GLFW_GAMEPAD_BUTTON_LEFT_BUMPER;
    mBindings["powerslide"].button = GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER;
    mBindings["ballcam"].button = GLFW_GAMEPAD_BUTTON_Y;
    mBindings["airRollLeft"].button = GLFW_GAMEPAD_BUTTON_X;
    mBindings["airRollRight"].button = GLFW_GAMEPAD_BUTTON_B;
    mBindings["lookBehind"].button = GLFW_GAMEPAD_BUTTON_RIGHT_THUMB;

    GamepadBinding pitch;
    pitch.axis = GLFW_GAMEPAD_AXIS_LEFT_Y;
    pitch.invert = true;
    pitch.deadzone = 0.10f;
    mBindings["pitch"] = pitch;

    GamepadBinding swivelX;
    swivelX.axis = GLFW_GAMEPAD_AXIS_RIGHT_X;
    swivelX.deadzone = 0.12f;
    mBindings["swivelX"] = swivelX;

    GamepadBinding swivelY;
    swivelY.axis = GLFW_GAMEPAD_AXIS_RIGHT_Y;
    swivelY.invert = true;
    swivelY.deadzone = 0.12f;
    mBindings["swivelY"] = swivelY;
}

bool GamepadInput::reloadConfig()
{
    applyDefaultBindings();

    std::ifstream file(mConfigPath);
    if (!file.is_open())
    {
        loadMappings();
        return false;
    }

    try
    {
        const json root = parseJsonConfig(file);
        if (!root.is_object())
        {
            loadMappings();
            return false;
        }
        mMappingsPath = root.value("mappings_file", mMappingsPath);
        const float defaultDeadzone =
            root.value("deadzone", 0.15f);
        if (root.contains("bindings") && root["bindings"].is_object())
        {
            for (auto it = root["bindings"].begin();
                 it != root["bindings"].end(); ++it)
            {
                if (!it.value().is_object())
                    continue;
                GamepadBinding& binding = mBindings[it.key()];
                const json& node = it.value();
                binding.button = node.value("button", binding.button);
                binding.axis = node.value("axis", binding.axis);
                binding.trigger = node.value("trigger", binding.trigger);
                binding.invert = node.value("invert", binding.invert);
                binding.deadzone =
                    node.value("deadzone", defaultDeadzone);
                binding.upperDeadzone =
                    node.value("upperDeadzone", binding.upperDeadzone);
            }
        }
        loadMappings();
        return true;
    }
    catch (const std::exception& e)
    {
        Debug::warn(Debug::Category::General,
                    "[GAMEPAD] failed to load %s: %s\n",
                    mConfigPath.c_str(), e.what());
        applyDefaultBindings();
        loadMappings();
        return false;
    }
}

void GamepadInput::loadMappings()
{
    if (!ensureGamepadGlfw())
    {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "input.gamepad.mappings", "gamepad",
            "controller mapping database skipped because GLFW is unavailable",
            0,
            {{"path", mMappingsPath},
             {"loaded", false},
             {"result", "glfw_unavailable"}});
        return;
    }

    std::ifstream file(mMappingsPath);
    if (!file.is_open())
    {
        Debug::warn(Debug::Category::General,
                    "[GAMEPAD] mappings file missing: %s\n",
                    mMappingsPath.c_str());
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "input.gamepad.mappings", "gamepad",
            "controller mapping database was not found",
            0,
            {{"path", mMappingsPath},
             {"loaded", false},
             {"result", "missing"}});
        return;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string mappings = buffer.str();

    // Load line by line. This keeps one malformed or platform-specific entry
    // from rejecting the entire database and makes the accepted/rejected count
    // observable in the runtime journal.
    std::istringstream lines(mappings);
    std::string line;
    int accepted = 0;
    int rejected = 0;
    while (std::getline(lines, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty() || line[0] == '#')
            continue;
        if (glfwUpdateGamepadMappings(line.c_str()))
            ++accepted;
        else
            ++rejected;
    }

    glfwGetError(nullptr);  // clear an earlier stale error before reporting
    const char* glfwError = nullptr;
    glfwGetError(&glfwError);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "input.gamepad.mappings", "gamepad",
        "controller mapping database load result",
        0,
        {{"path", mMappingsPath},
         {"loaded", accepted > 0},
         {"accepted", accepted},
         {"rejected", rejected},
         {"bytes", mappings.size()},
         {"glfw_error", glfwError ? glfwError : ""},
         {"result", accepted > 0 ? "ok" : "rejected"}});
    Debug::log(Debug::Category::General,
               "[GAMEPAD] mapping database accepted=%d rejected=%d (%s)\n",
               accepted, rejected, mMappingsPath.c_str());

    // One bounded device census after mappings load. This distinguishes "no
    // physical device present" from "device present but unmapped", which the
    // later logical-action state alone cannot show.
    for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid)
    {
        if (!glfwJoystickPresent(jid))
            continue;
        const char* name = glfwGetJoystickName(jid);
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "input.gamepad.device", "gamepad",
            "physical joystick detected", 0,
            {{"joystick_id", jid},
             {"name", name ? name : ""},
             {"standard_mapping", glfwJoystickIsGamepad(jid) != 0}});
    }
}

void GamepadInput::setVirtualState(float throttle, float brake, float steer,
                                   bool jump, bool boost, bool powerslide,
                                   bool ballcam)
{
    mVirtual = true;
    mVirtualThrottle = std::clamp(throttle, 0.0f, 1.0f);
    mVirtualBrake = std::clamp(brake, 0.0f, 1.0f);
    mVirtualSteer = std::clamp(steer, -1.0f, 1.0f);
    mVirtualJump = jump;
    mVirtualBoost = boost;
    mVirtualPowerslide = powerslide;
    mVirtualBallcam = ballcam;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "input.gamepad.virtual", "gamepad",
        "synthetic gamepad state enabled for bounded controller-path proof", 0,
        {{"throttle", mVirtualThrottle},
         {"brake", mVirtualBrake},
         {"steer", mVirtualSteer},
         {"jump", mVirtualJump},
         {"boost", mVirtualBoost},
         {"powerslide", mVirtualPowerslide},
         {"ballcam", mVirtualBallcam}});
}

void GamepadInput::clearVirtualState()
{
    if (!mVirtual)
        return;
    mVirtual = false;
    mVirtualThrottle = 0.0f;
    mVirtualBrake = 0.0f;
    mVirtualSteer = 0.0f;
    mVirtualJump = false;
    mVirtualBoost = false;
    mVirtualPowerslide = false;
    mVirtualBallcam = false;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "input.gamepad.virtual", "gamepad",
        "synthetic gamepad state disabled", 0,
        {{"active", false}});
}

void GamepadInput::logState(const char* reason)
{
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "input.gamepad.state", "gamepad",
        reason ? reason : "gamepad state snapshot", 0,
        {{"connected", mConnected},
         {"virtual", mVirtual},
         {"name", mName},
         {"device_id", mDeviceId},
         {"standard_mapping", mStandardMapping},
         {"raw_fallback", mRawOnly},
         {"axis_count", mAxisCount},
         {"button_count", mButtonCount},
         {"throttle", value("throttle")},
         {"brake", value("brake")},
         {"steer", value("steer")},
         {"jump", held("jump")},
         {"boost", held("boost")},
         {"powerslide", held("powerslide")},
         {"ballcam", held("ballcam")}});
}

void GamepadInput::poll()
{
    if (!ensureGamepadGlfw())
    {
        mConnected = false;
        return;
    }
    if (mVirtual)
    {
        mConnected = true;
        mStandardMapping = true;
        mRawOnly = false;
        mDeviceId = -1;
        mName = "virtual-gamepad";
        mAxisCount = 0;
        mButtonCount = 0;
        return;
    }

    mConnected = false;
    mStandardMapping = false;
    mRawOnly = false;
    mDeviceId = -1;
    mName.clear();
    mAxisCount = 0;
    mButtonCount = 0;

    int device = -1;
    for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid)
    {
        if (glfwJoystickPresent(jid))
        {
            device = jid;
            break;
        }
    }
    if (device < 0)
    {
        if (mLoggedConnectionState)
        {
            mLoggedConnectionState = false;
            logState("gamepad disconnected");
        }
        return;
    }

    mConnected = true;
    mDeviceId = device;
    const char* rawName = glfwGetJoystickName(device);
    mName = rawName ? rawName : "gamepad";

    if (!mLoggedConnectionState)
    {
        mLoggedConnectionState = true;
        logState("gamepad connected");
    }

    if (glfwJoystickIsGamepad(device))
    {
        GLFWgamepadstate state{};
        if (!glfwGetGamepadState(device, &state))
            return;
        mStandardMapping = true;
        mAxisCount = GLFW_GAMEPAD_AXIS_LAST;
        mButtonCount = GLFW_GAMEPAD_BUTTON_LAST;
        for (int i = 0; i < mAxisCount && i < 16; ++i)
            mAxes[i] = state.axes[i];
        for (int i = 0; i < mButtonCount && i < 32; ++i)
            mButtons[i] = state.buttons[i];
        return;
    }

    mRawOnly = true;
    int count = 0;
    const float* axes = glfwGetJoystickAxes(device, &count);
    mAxisCount = std::clamp(count, 0, 16);
    for (int i = 0; i < mAxisCount; ++i)
        mAxes[i] = axes ? axes[i] : 0.0f;

    count = 0;
    const unsigned char* buttons = glfwGetJoystickButtons(device, &count);
    mButtonCount = std::clamp(count, 0, 32);
    for (int i = 0; i < mButtonCount; ++i)
        mButtons[i] = buttons ? buttons[i] : 0;
}

float GamepadInput::axisValue(int axis, bool trigger, bool invert,
                              float deadzone, float upperDeadzone) const
{
    if (axis < 0 || axis >= mAxisCount || axis >= 16)
        return 0.0f;
    float value = mAxes[axis];
    if (!std::isfinite(value))
        return 0.0f;
    if (invert)
        value = -value;
    if (trigger)
    {
        // Raw trigger range is 0..1 for GLFW gamepads. Remap lower/upper
        // deadzones so a configurable travel reaches 100% early and the scale
        // stays linear in between.
        const float raw = std::clamp((value + 1.0f) * 0.5f, 0.0f, 1.0f);
        if (raw <= deadzone)
            return 0.0f;
        const float span = std::max(
            std::clamp(upperDeadzone, deadzone + 0.01f, 1.0f) - deadzone, 0.01f);
        return std::clamp((raw - deadzone) / span, 0.0f, 1.0f);
    }
    return applyDeadzone(value, deadzone, upperDeadzone);
}

bool GamepadInput::buttonValue(int button) const
{
    if (button < 0 || button >= mButtonCount || button >= 32)
        return false;
    return mButtons[button] != 0;
}

bool GamepadInput::held(const std::string& action) const
{
    if (!mConnected)
        return false;
    if (mVirtual)
    {
        if (action == "jump")
            return mVirtualJump;
        if (action == "boost")
            return mVirtualBoost;
        if (action == "powerslide")
            return mVirtualPowerslide;
        if (action == "ballcam")
            return mVirtualBallcam;
    }
    auto it = mBindings.find(action);
    if (it == mBindings.end())
        return false;
    const GamepadBinding& binding = it->second;
    if (binding.button >= 0)
        return buttonValue(binding.button);
    if (binding.axis >= 0)
        return axisValue(binding.axis, binding.trigger, binding.invert,
                         binding.deadzone, binding.upperDeadzone) > 0.35f;
    return false;
}

float GamepadInput::value(const std::string& action) const
{
    if (!mConnected)
        return 0.0f;
    if (mVirtual)
    {
        if (action == "throttle")
            return mVirtualThrottle;
        if (action == "brake")
            return mVirtualBrake;
        if (action == "steer")
            return mVirtualSteer;
        if (action == "jump")
            return mVirtualJump ? 1.0f : 0.0f;
        if (action == "boost")
            return mVirtualBoost ? 1.0f : 0.0f;
        if (action == "powerslide")
            return mVirtualPowerslide ? 1.0f : 0.0f;
        if (action == "ballcam")
            return mVirtualBallcam ? 1.0f : 0.0f;
    }
    auto it = mBindings.find(action);
    if (it == mBindings.end())
        return 0.0f;
    const GamepadBinding& binding = it->second;
    if (binding.axis >= 0)
        return axisValue(binding.axis, binding.trigger, binding.invert,
                         binding.deadzone, binding.upperDeadzone);
    if (binding.button >= 0)
        return buttonValue(binding.button) ? 1.0f : 0.0f;
    return 0.0f;
}
