# Tick Rate Rule (Hard Rule)

All gameplay collision, damage, and physics MUST run at the fixed 60Hz tick rate, never per-frame. The game uses `constexpr double kClientFixedDt = 1.0 / 60.0` in `engine-tick-combat.cpp`. Running collision per-frame means:
- Low-FPS devices get fewer collision checks (unfair disadvantage)
- High-FPS devices get more collision checks (unfair advantage)
- Gameplay behavior varies by hardware

Weapon collision, NPC overlap detection, damage application, and knockback MUST use the tick accumulator pattern:
```cpp
const float tickDt = 1.0f / 60.0f;
const uint32_t ticksThisFrame = std::max(1u, (uint32_t)std::round(dt / tickDt));
for (uint32_t t = 0; t < ticksThisFrame; t++) {
    // collision + damage logic here
}
```

Never run collision logic in the raw render loop without tick quantization.

# VSync Policy (Hard Rule — Never Override)

VSync is **forced OFF globally** and must never be re-enabled. This is a permanent performance constraint.

## Rules

1. **Never call `glfwSwapInterval(1)` anywhere in the codebase.** Every call must be `glfwSwapInterval(0)`.
2. **Never write `mVSync = true`** in any file (Renderer, VideoSettings, FramePacer).
3. **Never add a UI toggle, slider, or terminal command that enables VSync.**
4. **Never read `"vsync": true` from config and apply it.** Config loads must override any stored vsync value to `false`.
5. **Never add command-line flags that control VSync.**
6. **Never skip the software frame pacer** (`FramePacer::endFrame()`) by claiming "VSync handles pacing."
7. **Fullscreen/window transitions must always call `glfwSwapInterval(0)`** after `glfwSetWindowMonitor`.
8. **Any code path that calls `Renderer::setVSync(true)` must be blocked** with a `[VSYNC] BLOCKED` log.

## Why

VSync adds 16-29ms of driver-level stall per frame (measured at the Swap scope). The engine uses a software frame pacer (`FramePacer::endFrame()`) with `maxFrames` to control frame rate. VSync is incompatible with the performance target of <1ms frame times.

## Enforcement Points

- `Renderer::setVSync()` — always calls `glfwSwapInterval(0)`, logs `[VSYNC] BLOCKED` if `on==true`
- `Renderer::applyVideoMode()` — always calls `glfwSwapInterval(0)` after monitor changes
- `Renderer::forceVSyncOff()` — explicit force-off with reason logging
- `VideoSettings::setVSync()` — forces `mVSync = false`, logs blocked attempt
- `VideoSettings::load()` — overrides JSON `"vsync": true` to `false`
- `VideoSettings::save()` — always writes `"vsync": false`
- `FramePacer::setVSync()` — forces `mVSync = false`, logs blocked attempt
- Settings menu — shows "VSync: OFF (forced)" with no toggle
- Terminal `vsync` command — logs blocked, refuses to enable
- Boot sequence — loud `[VSYNC] FORCED OFF` log confirming state

## If You Think You Need VSync

You don't. Use `maxFrames` in the terminal or settings to cap FPS. If the frame rate is unstable, fix the frame pacer or reduce render cost — do not add VSync.

# Performance Rules

Prefer:

* predictable execution
* cache-friendly structures
* simple data flow
* low allocation counts
* low memory usage

Measure before optimizing.

But when two implementations are equal:

prefer the simpler and faster one.

Avoid architecture that exists only for abstraction.

Abstractions should earn their cost.

