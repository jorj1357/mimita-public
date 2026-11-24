// 2026-11-24

#include "input/gamepad-input.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>

#include <GLFW/glfw3.h>
#include <nlohmann/json.hpp>

#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "utils/json-comments.h"

using json = nlohmann::json;

namespace {

float applyDeadzone(float value, float deadzone)
{
    const float magnitude = std::fabs(value);
    if (magnitude <= deadzone)
        return 0.0f;
    const float scaled = (magnitude - deadzone) / std::max(1.0f - deadzone, 0.01f);
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
    mBindings["throttle"] = throttle;

    GamepadBinding brake;
    brake.axis = GLFW_GAMEPAD_AXIS_LEFT_TRIGGER;
    brake.trigger = true;
    brake.deadzone = 0.02f;
    mBindings["brake"] = brake;

    mBindings["jump"].button = GLFW_GAMEPAD_BUTTON_A;
    mBindings["boost"].button = GLFW_GAMEPAD_BUTTON_B;
    mBindings["powerslide"].button = GLFW_GAMEPAD_BUTTON_X;
    mBindings["ballcam"].button = GLFW_GAMEPAD_BUTTON_Y;
}

bool GamepadInput::reloadConfig()
{
    applyDefaultBindings();

    std::ifstream file(mConfigPath);
    if (!file.is_open())
        return false;

    try
    {
        const json root = parseJsonConfig(file);
        if (!root.is_object())
            return false;
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
            }
        }
        return true;
    }
    catch (const std::exception& e)
    {
        Debug::warn(Debug::Category::General,
                    "[GAMEPAD] failed to load %s: %s\n",
                    mConfigPath.c_str(), e.what());
        applyDefaultBindings();
        return false;
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
                              float deadzone) const
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
        value = std::clamp((value + 1.0f) * 0.5f, 0.0f, 1.0f);
        return value <= deadzone ? 0.0f : value;
    }
    return applyDeadzone(value, deadzone);
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
                         binding.deadzone) > 0.35f;
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
                         binding.deadzone);
    if (binding.button >= 0)
        return buttonValue(binding.button) ? 1.0f : 0.0f;
    return 0.0f;
}
