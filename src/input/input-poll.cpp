// 07 21 2026, 16 30
/* purpose
* Converts current input command state into InputState and InputFrame data.
* Builds camera-relative movement axes and edge-triggered gameplay action fields.
* Applies terminal input overrides before the fixed-step simulation consumes them.
* Does NOT run movement physics, send packets, decide authority, or own bindings.
* Does NOT render UI, mutate Player state, or perform replay serialization.
* Does NOT implement gameplay ability formulas or prediction history.
*/

#include "input/input-state.h"
#include "input/input-commands.h"
#include "input/input-frame.h"
#include "combat/melee-client.h"
#include "debug/structured-log.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include "camera.h"
#include "interaction/interaction-system.h"
#include "vehicles/vehicle-system.h"

static bool gInputInitialized = false;
InputFrame gTerminalInputOverride;

namespace {
int gScheduledMoveTicks = 0;
int gScheduledMoveTotal = 0;
bool gScheduledMoveNetworkReleasePending = false;
float gScheduledMoveX = 0.0f;
float gScheduledMoveY = 0.0f;
std::string gScheduledMoveRequestId;
std::string gTerminalInputOverrideRequestId;
}

void scheduleTerminalMove(float moveX, float moveY, int durationTicks,
                          const std::string& requestId)
{
    gScheduledMoveX = moveX;
    gScheduledMoveY = moveY;
    gScheduledMoveTicks = durationTicks > 0 ? durationTicks : 0;
    gScheduledMoveTotal = gScheduledMoveTicks;
    gScheduledMoveNetworkReleasePending = false;
    gScheduledMoveRequestId = requestId;
}

const std::string& scheduledTerminalMoveRequestId()
{
    return gScheduledMoveRequestId;
}

void noteTerminalInputOverrideRequestId(const std::string& requestId)
{
    gTerminalInputOverrideRequestId = requestId;
}

bool consumeScheduledTerminalMove(float& moveX, float& moveY, bool* justStarted,
                                  bool* justEnded)
{
    if (gScheduledMoveTicks <= 0)
        return false;
    moveX = gScheduledMoveX;
    moveY = gScheduledMoveY;
    if (justStarted)
        *justStarted = (gScheduledMoveTicks == gScheduledMoveTotal);
    --gScheduledMoveTicks;
    if (justEnded)
        *justEnded = (gScheduledMoveTicks == 0);
    return true;
}

bool advanceScheduledTerminalMoveFixedTick(bool* justStarted, bool* justEnded)
{
    if (gScheduledMoveTicks <= 0)
        return false;
    if (justStarted)
        *justStarted = (gScheduledMoveTicks == gScheduledMoveTotal);
    --gScheduledMoveTicks;
    if (justEnded)
        *justEnded = (gScheduledMoveTicks == 0);
    if (gScheduledMoveTicks == 0)
        gScheduledMoveNetworkReleasePending = true;
    return true;
}

bool consumeScheduledTerminalMoveForNetwork(float& moveX, float& moveY)
{
    if (gScheduledMoveTicks <= 0 && !gScheduledMoveNetworkReleasePending)
        return false;
    moveX = gScheduledMoveX;
    moveY = gScheduledMoveY;
    gScheduledMoveNetworkReleasePending = false;
    return true;
}

int scheduledTerminalMoveTotalTicks()
{
    return gScheduledMoveTotal;
}

bool peekScheduledTerminalMove(float& moveX, float& moveY, bool* justStarted)
{
    if (gScheduledMoveTicks <= 0)
        return false;
    moveX = gScheduledMoveX;
    moveY = gScheduledMoveY;
    if (justStarted)
        *justStarted = (gScheduledMoveTicks == gScheduledMoveTotal);
    return true;
}

void consumeTerminalInputOverride() {
    gTerminalInputOverride = InputFrame{};
}

static void ensureInputInit(GLFWwindow* win) {
    if (!gInputInitialized) {
        InputCommandSystem::instance().init(win);
        InputCommandSystem::instance().loadBinds("config/accounts/default.json");
        gInputInitialized = true;
    }
}

InputState pollInput(GLFWwindow* win, const Camera& cam)
{
    ensureInputInit(win);
    InputCommandSystem::instance().update(0.016f);
    
    InputState in;
    in.camForward = cam.front;

    glm::vec3 f = cam.front;
    f.z = 0.0f;
    if (glm::length(f) > 0.0001f)
        f = glm::normalize(f);

    glm::vec3 r = glm::normalize(glm::cross(f, glm::vec3(0,0,1)));

    glm::vec2 wish(0.0f);

    const auto& cmd = InputCommandSystem::instance();

    float scheduledSide = 0.0f;
    float scheduledForward = 0.0f;
    bool scheduledJustStarted = false;
    // pollInput owns the single per-frame consume of a scheduled move: its
    // output feeds the InputPacket sent to the authoritative server, so the
    // wish must not be consumed by the local-prediction path first.
    const bool scheduledMove = consumeScheduledTerminalMove(
        scheduledSide, scheduledForward, &scheduledJustStarted, nullptr);

    if (cmd.getState("walkforward").held) {
        wish.x += f.x;
        wish.y += f.y;
    }
    if (cmd.getState("walkback").held) {
        wish.x -= f.x;
        wish.y -= f.y;
    }
    if (cmd.getState("walkright").held) {
        wish.x += r.x;
        wish.y += r.y;
    }
    if (cmd.getState("walkleft").held) {
        wish.x -= r.x;
        wish.y -= r.y;
    }

    // 6 6 2026 also add this for normaliz 
    if (glm::length(wish) > 0.0001f)
        wish = glm::normalize(wish);

    // agent.move is scheduled at the same input boundary as keyboard input.
    // The previous code populated only gTerminalInputOverride, while the
    // network client serializes InputState::wishMoveXY, so the command was
    // acknowledged but sent a zero movement vector.
    if (scheduledMove) {
        wish = scheduledForward * glm::vec2(f.x, f.y) +
               scheduledSide * glm::vec2(r.x, r.y);
        if (glm::length(wish) > 0.0001f)
            wish = glm::normalize(wish);
    }

    in.wishMoveXY = wish;

    // Keyboard movement OR a scheduled agent.move keeps movementPressed true;
    // the previous code overwrote the scheduled flag with keyboard-only state,
    // so agent.move reported movementPressed=false.
    const bool keyboardMoving =
        cmd.getState("walkforward").held ||
        cmd.getState("walkback").held ||
        cmd.getState("walkleft").held ||
        cmd.getState("walkright").held;
    const bool keyboardJustPressed =
        cmd.getState("walkforward").pressed ||
        cmd.getState("walkback").pressed ||
        cmd.getState("walkleft").pressed ||
        cmd.getState("walkright").pressed;
    in.movementPressed = scheduledMove || keyboardMoving;
    in.movementJustPressed = scheduledJustStarted || keyboardJustPressed;
    
    in.movementHeldDuration = cmd.getMovementHoldDuration();
    in.jumpHeld = cmd.isJumpHeld();
    in.jumpPressed = cmd.getState("jump").pressed;
    in.dashPressed = cmd.isDashPressed();
    in.groundReturnPressed = cmd.isGroundReturnPressed();
    in.downDashPressed = cmd.isDownDashPressed();
    in.freezeHeld = cmd.isFreezeHeld();
    in.freezePressed = cmd.getState("freeze").pressed;
    in.crouchHeld = cmd.isCrouchHeld() || MimitaMelee::debugCrouchHeld();
    const bool vehicleInteraction =
        InteractionSystem::instance().handleKey(win, cam);
    in.reviveHeld = glfwGetKey(win, GLFW_KEY_F) == GLFW_PRESS &&
                    !vehicleInteraction &&
                    !VehicleSystem::instance().hasLocalDriver();

    if (in.dashPressed)
        printf("[INPUT] Dash pressed\n");

    // Ragdoll mode inputs
    in.ragdollTogglePressed = glfwGetKey(win, GLFW_KEY_G) == GLFW_PRESS;
    in.grabLeftHeld = glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS;
    in.grabRightHeld = glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS;
    in.extendLeftMouse = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    in.extendRightMouse = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    static bool fisterFistPrev = false;
    const bool fisterFist = in.extendLeftMouse;
    in.fisterTogglePressed = glfwGetKey(win, GLFW_KEY_F8) == GLFW_PRESS;
    in.fisterFistHeld = fisterFist;
    in.fisterFistPressed = fisterFist && !fisterFistPrev;
    in.fisterParryHeld = in.extendRightMouse;
    in.fisterGrabHeld = glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS;
    fisterFistPrev = fisterFist;

    return in;
}

InputFrame buildInputFrame(GLFWwindow* win, const Camera& cam)
{
    ensureInputInit(win);
    InputCommandSystem::instance().update(0.016f);

    const auto& cmd = InputCommandSystem::instance();

    glm::vec3 f = cam.front;
    f.z = 0.0f;
    if (glm::length(f) > 0.0001f)
        f = glm::normalize(f);
    glm::vec3 r = glm::normalize(glm::cross(f, glm::vec3(0,0,1)));

    InputFrame frame;

    // F enters/exits a target before movement is consumed. A valid target wins
    // over revive (revive is not part of the local InputFrame path).
    InteractionSystem::instance().handleKey(win, cam);

    // 6 6 2026 normalize again 
    glm::vec2 move(frame.moveX, frame.moveY);

    if (glm::length(move) > 0.0001f)
    {
        move = glm::normalize(move);

        frame.moveX = move.x;
        frame.moveY = move.y;
    }

    if (cmd.getState("walkforward").held) { frame.moveX += f.x; frame.moveY += f.y; }
    if (cmd.getState("walkback").held)    { frame.moveX -= f.x; frame.moveY -= f.y; }
    if (cmd.getState("walkright").held)   { frame.moveX += r.x; frame.moveY += r.y; }
    if (cmd.getState("walkleft").held)    { frame.moveX -= r.x; frame.moveY -= r.y; }

    // The fixed-step simulation reads this frame before the multiplayer
    // packet is assembled. Peek here so physics and animation see the
    // command; engineTickNet consumes one scheduled tick afterward.
    float scheduledSide = 0.0f;
    float scheduledForward = 0.0f;
    bool scheduledJustStarted = false;
    const bool scheduledMove =
        peekScheduledTerminalMove(scheduledSide, scheduledForward, &scheduledJustStarted);
    if (scheduledMove) {
        frame.moveX = scheduledForward * f.x + scheduledSide * r.x;
        frame.moveY = scheduledForward * f.y + scheduledSide * r.y;
    }

    const bool keyboardMoving =
        cmd.getState("walkforward").held ||
        cmd.getState("walkback").held ||
        cmd.getState("walkleft").held ||
        cmd.getState("walkright").held;
    const bool keyboardJustPressed =
        cmd.getState("walkforward").pressed ||
        cmd.getState("walkback").pressed ||
        cmd.getState("walkleft").pressed ||
        cmd.getState("walkright").pressed;
    frame.movementPressed = scheduledMove || keyboardMoving;
    frame.movementJustPressed = scheduledJustStarted || keyboardJustPressed;

    frame.jump = cmd.isJumpHeld() || gTerminalInputOverride.jump;
    frame.jumpPressed = cmd.getState("jump").pressed || gTerminalInputOverride.jumpPressed;
    frame.dashPressed = cmd.isDashPressed() || gTerminalInputOverride.dashPressed;
    frame.reloadPressed = cmd.getState("reload").pressed || gTerminalInputOverride.reloadPressed;
    frame.groundReturnPressed = cmd.isGroundReturnPressed() || gTerminalInputOverride.groundReturnPressed;
    frame.downDashPressed = cmd.isDownDashPressed() || gTerminalInputOverride.downDashPressed;
    frame.freezeHeld = cmd.isFreezeHeld() || gTerminalInputOverride.freezeHeld;
    frame.freezePressed = cmd.getState("freeze").pressed || gTerminalInputOverride.freezePressed;
    frame.crouchHeld = cmd.isCrouchHeld() || MimitaMelee::debugCrouchHeld() ||
        gTerminalInputOverride.crouchHeld;

    if (gTerminalInputOverride.moveX != 0.0f || gTerminalInputOverride.moveY != 0.0f) {
        frame.moveX = gTerminalInputOverride.moveX;
        frame.moveY = gTerminalInputOverride.moveY;
        frame.movementPressed = true;
        frame.movementJustPressed = true;
    }

    frame.lookYaw = (gTerminalInputOverride.lookYaw != 0.0f) ? gTerminalInputOverride.lookYaw : cam.yaw;
    frame.lookPitch = (gTerminalInputOverride.lookPitch != 0.0f) ? gTerminalInputOverride.lookPitch : cam.pitch;

    // Ragdoll mode inputs
    frame.ragdollTogglePressed = glfwGetKey(win, GLFW_KEY_G) == GLFW_PRESS;
    frame.grabLeftHeld = glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS;
    frame.grabRightHeld = glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS;
    frame.extendLeftMouse = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    frame.extendRightMouse = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    frame.fisterTogglePressed = glfwGetKey(win, GLFW_KEY_F8) == GLFW_PRESS;
    frame.fisterFistHeld = frame.extendLeftMouse;
    frame.fisterParryHeld = frame.extendRightMouse;
    frame.fisterGrabHeld = glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS;

    // Camera-relative axes for the authoritative input packet. moveX/moveY
    // remain the world-space movement consumed by local simulation.
    frame.wishForward = frame.moveX * f.x + frame.moveY * f.y;
    frame.wishSide = frame.moveX * r.x + frame.moveY * r.y;
    frame.camForward = cam.front;

    frame.primaryHeld =
        glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    frame.secondaryHeld =
        glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    frame.shieldHeld = glfwGetKey(win, GLFW_KEY_Q) == GLFW_PRESS;
    frame.grabHeld = glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS;
    frame.slotHeldMask = 0;
    for (int slot = 0; slot <= 9; ++slot)
    {
        const int key = slot == 0 ? GLFW_KEY_0 : GLFW_KEY_0 + slot;
        if (glfwGetKey(win, key) == GLFW_PRESS)
            frame.slotHeldMask |= static_cast<uint16_t>(1u << slot);
    }

    // agent.action evidence: prove the requested jump/dash edge reached the
    // fixed-tick input frame, not merely that the command handler accepted it.
    if (gTerminalInputOverride.jumpPressed || gTerminalInputOverride.dashPressed) {
        nlohmann::json fields = {
            {"request_id", gTerminalInputOverrideRequestId},
            {"command", "agent.action"},
            {"jump_pressed", gTerminalInputOverride.jumpPressed},
            {"dash_pressed", gTerminalInputOverride.dashPressed}};
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Avatar, StructuredLevel::Important,
            "agent.action.applied",
            gTerminalInputOverrideRequestId.empty()
                ? "agent-command" : gTerminalInputOverrideRequestId,
            "terminal input override merged into fixed-tick input frame", 0, fields,
            __FILE__, __LINE__, __FUNCTION__);
    }

    consumeTerminalInputOverride();
    return frame;
}
