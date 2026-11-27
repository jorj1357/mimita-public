// 2026-11-24

#include "input/gamepad-input.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

#include <GLFW/glfw3.h>
#include <nlohmann/json.hpp>

#if defined(_WIN32)
#include <setupapi.h>
#include <hidsdi.h>
#endif

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

#if defined(_WIN32)
// Raw DualShock 4 HID fallback.
//
// GLFW 3.4's Windows joystick backend can report a Bluetooth DS4 as present and
// standard-mapped while every axis stays at ~1.5e-05, even though Windows and
// other applications read the same device. This backend opens the Sony HID
// interface directly and decodes the DS4 input report into the existing GLFW
// standard mapping so the rest of the input path is unchanged.
//
// USB report 0x01: [id][lx][ly][rx][ry][btns0][btns1][btns2][l2][r2]...
// Bluetooth 0x11:  [id][0xc0][0x00][lx][ly][rx][ry][btns0][btns1][btns2][l2][r2]...
// The Bluetooth report is padded to the device's maximum input length; only the
// leading controller-state bytes are decoded.

uint64_t hidNowMs()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

struct HidDs4Device
{
    HANDLE handle = INVALID_HANDLE_VALUE;
    HANDLE event = nullptr;
    OVERLAPPED overlapped{};
    bool readPending = false;
    bool haveState = false;
    bool firstReportLogged = false;
    uint64_t lastAttemptMs = 0;
    uint64_t lastReportMs = 0;
    unsigned short vendorId = 0;
    unsigned short productId = 0;
    int inputLength = 0;
    std::string path;
    unsigned char buffer[1024] = {};
    float axes[6] = {};
    unsigned char buttons[15] = {};

    ~HidDs4Device() { close(); }

    void close()
    {
        if (handle != INVALID_HANDLE_VALUE)
        {
            if (readPending)
            {
                CancelIo(handle);
                readPending = false;
            }
            CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
        }
        if (event)
        {
            CloseHandle(event);
            event = nullptr;
        }
        haveState = false;
        firstReportLogged = false;
    }
};

HidDs4Device& hidDs4()
{
    static HidDs4Device device;
    return device;
}

bool hidDs4Parse(HidDs4Device& device, const unsigned char* report,
                 unsigned long length)
{
    if (length < 12)
        return false;
    int base = 0;
    const char* mode = "unknown";
    if (report[0] == 0x01)
    {
        base = 1;
        mode = "usb";
    }
    else if (report[0] == 0x11)
    {
        base = 3;
        mode = "bluetooth";
    }
    else
    {
        return false;
    }
    if (length < static_cast<unsigned long>(base + 9))
        return false;

    const auto stick = [](unsigned char value) {
        return std::clamp((static_cast<float>(value) - 128.0f) / 127.0f,
                          -1.0f, 1.0f);
    };
    device.axes[0] = stick(report[base + 0]);  // left X
    device.axes[1] = stick(report[base + 1]);  // left Y (up is negative)
    device.axes[2] = stick(report[base + 2]);  // right X
    device.axes[3] = stick(report[base + 3]);  // right Y
    device.axes[4] = report[base + 7] / 255.0f;  // L2
    device.axes[5] = report[base + 8] / 255.0f;  // R2

    std::memset(device.buttons, 0, sizeof(device.buttons));
    const unsigned char b0 = report[base + 4];
    const unsigned char b1 = report[base + 5];
    const unsigned char b2 = report[base + 6];
    if (b0 & 0x10) device.buttons[2] = 1;   // square -> X
    if (b0 & 0x20) device.buttons[0] = 1;   // cross  -> A
    if (b0 & 0x40) device.buttons[1] = 1;   // circle -> B
    if (b0 & 0x80) device.buttons[3] = 1;   // triangle -> Y
    if (b1 & 0x01) device.buttons[4] = 1;   // L1
    if (b1 & 0x02) device.buttons[5] = 1;   // R1
    if (b1 & 0x10) device.buttons[6] = 1;   // share -> back
    if (b1 & 0x20) device.buttons[7] = 1;   // options -> start
    if (b2 & 0x01) device.buttons[8] = 1;   // PS -> guide
    if (b1 & 0x40) device.buttons[9] = 1;   // L3
    if (b1 & 0x80) device.buttons[10] = 1;  // R3
    const int dpad = b0 & 0x0F;             // 0=N,1=NE,2=E,...,7=NW,8=released
    if (dpad != 8)
    {
        if (dpad == 0 || dpad == 1 || dpad == 7) device.buttons[11] = 1;
        if (dpad == 2 || dpad == 1 || dpad == 3) device.buttons[12] = 1;
        if (dpad == 4 || dpad == 3 || dpad == 5) device.buttons[13] = 1;
        if (dpad == 6 || dpad == 5 || dpad == 7) device.buttons[14] = 1;
    }
    device.haveState = true;

    if (!device.firstReportLogged)
    {
        device.firstReportLogged = true;
        char hex[3 * 16 + 1] = {};
        const int count = std::min<int>(
            16, static_cast<int>(length));
        for (int i = 0; i < count; ++i)
            std::snprintf(hex + i * 3, 4, "%02x ", report[i]);
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "input.gamepad.hid.report", "gamepad",
            "first raw DualShock 4 HID report decoded", 0,
            {{"report_id", report[0]},
             {"mode", mode},
             {"length", length},
             {"raw", hex},
             {"lx", device.axes[0]},
             {"ly", device.axes[1]},
             {"rx", device.axes[2]},
             {"ry", device.axes[3]},
             {"l2", device.axes[4]},
             {"r2", device.axes[5]}});
    }
    return true;
}

bool hidDs4TryOpen(HidDs4Device& device)
{
    if (device.handle != INVALID_HANDLE_VALUE)
        return true;
    const uint64_t now = hidNowMs();
    if (device.lastAttemptMs != 0 && now - device.lastAttemptMs < 2000)
        return false;
    device.lastAttemptMs = now;

    GUID guid{};
    HidD_GetHidGuid(&guid);
    HDEVINFO info = SetupDiGetClassDevsA(
        &guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (info == INVALID_HANDLE_VALUE)
        return false;

    bool opened = false;
    for (DWORD index = 0; !opened; ++index)
    {
        SP_DEVICE_INTERFACE_DATA iface{};
        iface.cbSize = sizeof(iface);
        if (!SetupDiEnumDeviceInterfaces(info, nullptr, &guid, index, &iface))
            break;

        DWORD required = 0;
        SetupDiGetDeviceInterfaceDetailA(
            info, &iface, nullptr, 0, &required, nullptr);
        if (required == 0)
            continue;
        std::vector<unsigned char> storage(required);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_A*>(
            storage.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
        if (!SetupDiGetDeviceInterfaceDetailA(
                info, &iface, detail, required, &required, nullptr))
            continue;

        HANDLE probe = CreateFileA(
            detail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING, 0, nullptr);
        if (probe == INVALID_HANDLE_VALUE)
            continue;
        HIDD_ATTRIBUTES attributes{};
        attributes.Size = sizeof(attributes);
        const bool sony =
            HidD_GetAttributes(probe, &attributes) &&
            attributes.VendorID == 0x054C &&
            (attributes.ProductID == 0x09CC || attributes.ProductID == 0x05C4);
        CloseHandle(probe);
        if (!sony)
            continue;

        HANDLE handle = CreateFileA(
            detail->DevicePath, GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            handle = CreateFileA(
                detail->DevicePath, GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                FILE_FLAG_OVERLAPPED, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            continue;
        HANDLE event = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        if (!event)
        {
            CloseHandle(handle);
            continue;
        }

        device.close();
        device.handle = handle;
        device.event = event;
        device.overlapped = OVERLAPPED{};
        device.overlapped.hEvent = event;
        device.readPending = false;
        device.haveState = false;
        device.firstReportLogged = false;
        device.lastReportMs = 0;
        device.vendorId = attributes.VendorID;
        device.productId = attributes.ProductID;
        device.inputLength = static_cast<int>(sizeof(device.buffer));
        device.path = detail->DevicePath;
        opened = true;

        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "input.gamepad.hid.selected", "gamepad",
            "raw DualShock 4 HID selected because GLFW reported an idle device",
            0,
            {{"path", device.path},
             {"vendor_id", device.vendorId},
             {"product_id", device.productId},
             {"input_length", device.inputLength}});
    }
    SetupDiDestroyDeviceInfoList(info);
    return opened;
}

bool hidDs4Read(HidDs4Device& device, float* axes, unsigned char* buttons)
{
    if (device.handle == INVALID_HANDLE_VALUE)
        return false;

    int parsed = 0;
    for (int i = 0; i < 4; ++i)
    {
        if (!device.readPending)
        {
            ResetEvent(device.event);
            device.overlapped = OVERLAPPED{};
            device.overlapped.hEvent = device.event;
            DWORD read = 0;
            if (!ReadFile(device.handle, device.buffer, sizeof(device.buffer),
                          &read, &device.overlapped))
            {
                if (GetLastError() != ERROR_IO_PENDING)
                {
                    device.close();
                    return false;
                }
                device.readPending = true;
            }
            else
            {
                if (read > 0 && hidDs4Parse(device, device.buffer, read))
                    ++parsed;
                continue;
            }
        }
        if (!device.readPending)
            continue;
        const DWORD wait = WaitForSingleObject(device.event, 0);
        if (wait == WAIT_TIMEOUT)
            break;
        DWORD read = 0;
        if (wait != WAIT_OBJECT_0 ||
            !GetOverlappedResult(device.handle, &device.overlapped, &read,
                                 FALSE))
        {
            device.readPending = false;
            device.close();
            return false;
        }
        device.readPending = false;
        if (read > 0 && hidDs4Parse(device, device.buffer, read))
            ++parsed;
    }

    if (!device.haveState)
        return false;
    const uint64_t now = hidNowMs();
    if (parsed > 0)
        device.lastReportMs = now;
    else if (now - device.lastReportMs > 1000)
    {
        // The pad stopped sending. Let GLFW try again on the next poll.
        device.close();
        return false;
    }

    for (int i = 0; i < 6; ++i)
        axes[i] = device.axes[i];
    for (int i = 0; i < 15; ++i)
        buttons[i] = device.buttons[i];
    return true;
}
#endif  // _WIN32

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
        mCaptureMode = root.value("capture_mode", mCaptureMode);
        if (mCaptureMode != "always" && mCaptureMode != "focused")
            mCaptureMode = "focused";
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "input.gamepad.capture_mode", "gamepad",
            "gamepad capture policy loaded", 0,
            {{"capture_mode", mCaptureMode},
             {"default", "focused"},
             {"config", mConfigPath}});
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
                binding.mode = node.value("mode", binding.mode);
                binding.sign = node.value("sign", binding.sign);
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

void GamepadInput::updateTriggerCalibration()
{
    constexpr int kCalibrationFrames = 30;
    for (int axis = 0; axis < mAxisCount && axis < 16; ++axis)
    {
        const float observed = mAxes[axis];
        if (!std::isfinite(observed))
            continue;
        AxisCalibration& cal = mAxisCal[axis];
        if (mCalibrationFrames == 0)
        {
            cal.rest = observed;
            cal.lo = observed;
            cal.hi = observed;
        }
        else
        {
            // The rest value is captured on the first frame and never moves;
            // travel keeps learning so a later full press reaches exactly 1.
            cal.lo = std::min(cal.lo, observed);
            cal.hi = std::max(cal.hi, observed);
        }
    }
    if (mCalibrationFrames < kCalibrationFrames)
    {
        ++mCalibrationFrames;
        if (mCalibrationFrames == kCalibrationFrames)
            finishTriggerCalibration();
    }
}

void GamepadInput::finishTriggerCalibration()
{
    nlohmann::json axes = nlohmann::json::array();
    for (int axis = 0; axis < mAxisCount && axis < 16; ++axis)
    {
        AxisCalibration& cal = mAxisCal[axis];
        const float travel = cal.hi - cal.lo;
        const char* convention = "axis01";
        if (travel < 0.25f)
        {
            // Nothing was pressed during the window: infer from the rest value.
            if (cal.rest <= -0.5f)
            {
                cal.lo = -1.0f;
                cal.hi = 1.0f;
                convention = "signed";
            }
            else if (cal.rest >= 0.5f)
            {
                cal.lo = 1.0f;
                cal.hi = -1.0f;
                convention = "inverted";
            }
            else
            {
                cal.lo = 0.0f;
                cal.hi = 1.0f;
                convention = "axis01";
            }
        }
        else if (cal.rest <= -0.5f)
        {
            convention = "signed";
        }
        else if (cal.rest >= 0.5f)
        {
            convention = "inverted";
        }
        cal.valid = true;
        axes.push_back({{"axis", axis},
                        {"rest", cal.rest},
                        {"lo", cal.lo},
                        {"hi", cal.hi},
                        {"convention", convention},
                        {"valid", true}});
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "input.gamepad.calibrated", "gamepad",
        "trigger rest and travel measured for this device", 0,
        {{"device_id", mDeviceId},
         {"name", mName},
         {"axis_count", mAxisCount},
         {"throttle_axis", 5},
         {"brake_axis", 4},
         {"throttle", value("throttle")},
         {"brake", value("brake")},
         {"axes", axes}});
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
         {"raw_throttle_axis", mAxisCount > 5 ? mAxes[5] : 0.0f},
         {"raw_brake_axis", mAxisCount > 4 ? mAxes[4] : 0.0f},
         {"trigger_calibrated", mAxisCal[4].valid && mAxisCal[5].valid},
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

    // Mode "focused" (default) owns the pad only while the game window has OS
    // focus. Mode "always" is the old behaviour: the first MiMITA process that
    // opens the pad reads it even while another application is in front.
    static bool sPadMuted = false;
    if (mCaptureMode == "focused" && !mWindowFocused)
    {
        if (!sPadMuted)
        {
            sPadMuted = true;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::General, StructuredLevel::Important,
                "input.gamepad.muted", "gamepad",
                "gamepad capture muted while the game window is unfocused", 0,
                {{"capture_mode", mCaptureMode},
                 {"window_focused", mWindowFocused}});
        }
        for (int i = 0; i < 16; ++i)
            mAxes[i] = 0.0f;
        for (int i = 0; i < 32; ++i)
            mButtons[i] = 0;
        return;
    }
    if (sPadMuted)
    {
        sPadMuted = false;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "input.gamepad.resumed", "gamepad",
            "gamepad capture resumed after focus", 0,
            {{"capture_mode", mCaptureMode},
             {"window_focused", mWindowFocused}});
    }

#if defined(_WIN32)
    // Raw-HID fallback: a Bluetooth DS4 can be present and standard-mapped in
    // GLFW while every GLFW axis stays idle. Once selected, the raw HID state
    // is authoritative and the GLFW path is only a fallback.
    auto applyHidState = [this]() -> bool
    {
        HidDs4Device& hid = hidDs4();
        if (hid.handle == INVALID_HANDLE_VALUE)
            return false;
        if (!hidDs4Read(hid, mAxes, mButtons))
            return false;
        mConnected = true;
        mStandardMapping = false;
        mRawOnly = false;
        mDeviceId = -2;
        mName = "Wireless Controller (raw HID)";
        mAxisCount = 6;
        mButtonCount = 15;
        for (int i = mAxisCount; i < 16; ++i)
            mAxes[i] = 0.0f;
        for (int i = mButtonCount; i < 32; ++i)
            mButtons[i] = 0;
        if (mCalibratedDeviceId != mDeviceId)
        {
            mCalibratedDeviceId = mDeviceId;
            mCalibrationFrames = 0;
            for (AxisCalibration& cal : mAxisCal)
                cal = AxisCalibration{};
            mLoggedConnectionState = false;
        }
        if (!mLoggedConnectionState)
        {
            mLoggedConnectionState = true;
            logState("gamepad connected (raw HID)");
        }
        updateTriggerCalibration();
        logAxes();
        logPoll();
        return true;
    };
    auto selectHid = [this, &applyHidState]() -> bool
    {
        if (!hidDs4TryOpen(hidDs4()))
            return false;
        return applyHidState();
    };
    if (applyHidState())
        return;
#endif

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
#if defined(_WIN32)
        // GLFW may not enumerate the pad at all; the raw HID path is
        // independent of it.
        if (selectHid())
            return;
#endif
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

    if (mCalibratedDeviceId != device)
    {
        // New device (or first poll): measure trigger rest and travel before
        // trusting any trigger value.
        mCalibratedDeviceId = device;
        mCalibrationFrames = 0;
        for (AxisCalibration& cal : mAxisCal)
            cal = AxisCalibration{};
    }

    if (glfwJoystickIsGamepad(device))
    {
        GLFWgamepadstate state{};
        if (glfwGetGamepadState(device, &state))
        {
        mStandardMapping = true;
        mAxisCount = GLFW_GAMEPAD_AXIS_LAST + 1;
        mButtonCount = GLFW_GAMEPAD_BUTTON_LAST + 1;
        bool standardActive = false;
        for (int i = 0; i < mAxisCount && i < 16; ++i)
        {
            mAxes[i] = state.axes[i];
            if (std::fabs(mAxes[i]) > 0.2f)
                standardActive = true;
        }
        for (int i = 0; i < mButtonCount && i < 32; ++i)
        {
            mButtons[i] = state.buttons[i];
            if (mButtons[i])
                standardActive = true;
        }

        // A Bluetooth "Wireless Controller" can be present with a standard
        // mapping while GLFW reports every axis as zero. Read the raw joystick
        // state too; if it shows activity while the standard state is idle, use
        // the raw device for the rest of the session. PS4 raw face buttons are
        // Square, Cross, Circle, Triangle; remap them to the standard
        // Cross, Circle, Square, Triangle order.
        static int sRawLatchDevice = -1;
        static float sPrevRawAxes[16] = {};
        int rawAxisCount = 0;
        const float* rawAxes = glfwGetJoystickAxes(device, &rawAxisCount);
        int rawButtonCount = 0;
        const unsigned char* rawButtons =
            glfwGetJoystickButtons(device, &rawButtonCount);
        bool rawActive = sRawLatchDevice == device;
        for (int i = 0; i < rawAxisCount && i < 16; ++i)
        {
            const float value = rawAxes ? rawAxes[i] : 0.0f;
            if (i < 4)
            {
                if (std::fabs(value) > 0.25f)
                    rawActive = true;
            }
            else if (std::fabs(value - sPrevRawAxes[i]) > 0.15f)
            {
                rawActive = true;
            }
            sPrevRawAxes[i] = value;
        }
        for (int i = 0; i < rawButtonCount && i < 32; ++i)
            if (rawButtons && rawButtons[i])
                rawActive = true;

        if (!standardActive && rawActive)
        {
            sRawLatchDevice = device;
            mStandardMapping = false;
            mRawOnly = true;
            mAxisCount = std::clamp(rawAxisCount, 0, 16);
            for (int i = 0; i < mAxisCount; ++i)
                mAxes[i] = rawAxes ? rawAxes[i] : 0.0f;
            mButtonCount = std::clamp(rawButtonCount, 0, 32);
            for (int i = 0; i < 32; ++i)
                mButtons[i] = 0;
            const bool ps4 = mName.find("Wireless Controller") !=
                             std::string::npos;
            static const int kPs4FaceToStandard[4] = {2, 0, 1, 3};
            for (int i = 0; i < mButtonCount; ++i)
            {
                const unsigned char pressed =
                    rawButtons ? rawButtons[i] : 0;
                if (ps4 && i < 4)
                    mButtons[kPs4FaceToStandard[i]] = pressed;
                else
                    mButtons[i] = pressed;
            }
            StructuredLogger::instance().writeEvent(
                StructuredCategory::General, StructuredLevel::Important,
                "input.gamepad.raw.selected", std::to_string(device),
                "standard gamepad state idle; raw Bluetooth axes selected", 0,
                {{"device_id", device},
                 {"name", mName},
                 {"axis_count", mAxisCount},
                 {"button_count", mButtonCount},
                 {"ps4_remap", ps4}});
            updateTriggerCalibration();
            logAxes();
            logPoll();
            return;
        }
#if defined(_WIN32)
        // GLFW reports this pad as present but idle. Try the raw HID backend
        // before accepting a zero state as real.
        if (!standardActive && !rawActive && selectHid())
            return;
#endif
        if (standardActive)
            sRawLatchDevice = -1;

        updateTriggerCalibration();
        logAxes();
        logPoll();
        return;
        }
        // The read failed. Do not give up: fall through to the raw joystick
        // axes/buttons, which is how a "Wireless Controller" with no GLFW
        // mapping was detected but never read (axis_count 0, button_count 0,
        // standard_mapping false, raw_fallback false in the live state).
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "input.gamepad.read_failed", std::to_string(device),
            "gamepad state read failed; using raw joystick axes", 0,
            {{"device_id", device}});
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
#if defined(_WIN32)
    bool glfwRawActive = false;
    for (int i = 0; i < mAxisCount; ++i)
        if (std::fabs(mAxes[i]) > 0.2f)
            glfwRawActive = true;
    for (int i = 0; i < mButtonCount; ++i)
        if (mButtons[i])
            glfwRawActive = true;
    if (!glfwRawActive && selectHid())
        return;
#endif
    updateTriggerCalibration();
    logAxes();
    logPoll();
}

float GamepadInput::axisValue(int axis, bool trigger, bool invert,
                              float deadzone, float upperDeadzone,
                              const std::string& mode, float sign) const
{
    if (axis < 0 || axis >= mAxisCount || axis >= 16)
        return 0.0f;
    float value = mAxes[axis];
    if (!std::isfinite(value))
        return 0.0f;

    const auto remap01 = [&](float raw) {
        if (raw <= deadzone)
            return 0.0f;
        const float span = std::max(
            std::clamp(upperDeadzone, deadzone + 0.01f, 1.0f) - deadzone,
            0.01f);
        return std::clamp((raw - deadzone) / span, 0.0f, 1.0f);
    };

    // Calibrated triggers ("auto"): released is exactly where the axis rests,
    // full travel is the observed extreme. This is what stops a pad whose
    // triggers rest at 0 from reading as half pressed forever.
    if (trigger && mode == "auto")
    {
        const AxisCalibration& cal = mAxisCal[axis];
        if (!cal.valid)
            return 0.0f;
        const float travel = cal.hi - cal.lo;
        if (std::fabs(travel) <= 1e-3f)
            return 0.0f;
        return remap01(std::clamp((value - cal.rest) / travel, 0.0f, 1.0f));
    }

    if (invert)
        value = -value;

    // "axis01": the device already reports a 0..1 axis (a DS4 over Bluetooth
    // does; treating that as -1..1 put both triggers at 0.5 at rest, which made
    // drive = throttle - brake cancel out).
    if (mode == "axis01")
        return remap01(std::clamp(value, 0.0f, 1.0f));

    // "combined": both triggers on one axis, one half each.
    if (mode == "combined")
    {
        const float raw = std::clamp(value * sign, 0.0f, 1.0f);
        return remap01(raw);
    }

    if (trigger)
    {
        // Legacy: signed axis where -1 is released.
        const float raw = std::clamp((value + 1.0f) * 0.5f, 0.0f, 1.0f);
        return remap01(raw);
    }
    return applyDeadzone(value, deadzone, upperDeadzone);
}

void GamepadInput::logPoll()
{
    // Once per second, regardless of whether anything moved: a dead pad and a
    // pad whose axes GLFW cannot see look identical without this. The device is
    // a Bluetooth "Wireless Controller", which can be present but expose zero
    // axes; this records exactly what GLFW reports.
    static uint64_t sLastMs = 0;
    const uint64_t nowMs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
    if (nowMs - sLastMs < 1000)
        return;
    sLastMs = nowMs;
    nlohmann::json axes = nlohmann::json::array();
    for (int i = 0; i < mAxisCount && i < 8; ++i)
        axes.push_back(mAxes[i]);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "input.gamepad.poll", "gamepad", "raw gamepad poll snapshot", 0,
        {{"connected", mConnected},
         {"virtual", mVirtual},
         {"name", mName},
         {"device_id", mDeviceId},
         {"is_gamepad", mDeviceId >= 0 ? glfwJoystickIsGamepad(mDeviceId) != 0 : false},
         {"standard_mapping", mStandardMapping},
         {"raw_fallback", mRawOnly},
         {"axis_count", mAxisCount},
         {"button_count", mButtonCount},
         {"axes", axes},
         {"throttle", value("throttle")},
         {"brake", value("brake")}});
}

void GamepadInput::logAxes()
{
    // Bounded capture of raw device state. This is the only way to tell a
    // mis-decoded trigger from a missing device, and it needs no build.
    static uint64_t sLastCaptureMs = 0;
    static float sPrevAxes[16] = {};
    static unsigned char sPrevButtons[32] = {};
    const uint64_t nowMs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
    bool changed = false;
    for (int i = 0; i < mAxisCount && i < 16; ++i)
        if (std::fabs(mAxes[i] - sPrevAxes[i]) > 0.15f)
            changed = true;
    for (int i = 0; i < mButtonCount && i < 32; ++i)
        if (mButtons[i] != sPrevButtons[i])
            changed = true;
    if (!changed || nowMs - sLastCaptureMs < 250)
        return;
    sLastCaptureMs = nowMs;
    for (int i = 0; i < 16; ++i)
        sPrevAxes[i] = mAxes[i];
    for (int i = 0; i < 32; ++i)
        sPrevButtons[i] = mButtons[i];

    nlohmann::json axes = nlohmann::json::array();
    for (int i = 0; i < mAxisCount && i < 16; ++i)
        axes.push_back(mAxes[i]);
    unsigned int buttonBits = 0;
    for (int i = 0; i < mButtonCount && i < 32; ++i)
        if (mButtons[i])
            buttonBits |= (1u << i);

    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "input.gamepad.axes", "gamepad",
        "raw gamepad axes changed", 0,
        {{"device", mName},
         {"standard_mapping", mStandardMapping},
         {"raw_only", mRawOnly},
         {"axis_count", mAxisCount},
         {"axes", axes},
         {"buttons", buttonBits},
         {"throttle", throttle()},
         {"brake", brake()},
         {"steer", steer()}});
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
                         binding.deadzone, binding.upperDeadzone,
                         binding.mode, binding.sign) > 0.35f;
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
                         binding.deadzone, binding.upperDeadzone,
                         binding.mode, binding.sign);
    if (binding.button >= 0)
        return buttonValue(binding.button) ? 1.0f : 0.0f;
    return 0.0f;
}
