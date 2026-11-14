# Input layer (2026-11-14)

Purpose: define the cold/hot input split and the path to any device, OS, or
controller without rewriting gameplay.

## Split

Cold (resident host):

- OS/window/device polling, hotplug, focus, raw key codes;
- gamepad/touch/pointer APIs;
- platform backends (GLFW now; SDL, XInput, evdev, browser, console later);
- the event queue and transport to the active generation.

Hot (game DLL):

- action bindings (`bindings.json`), chords, deadzones, sensitivity, invert;
- gameplay action mapping;
- UI/terminal/chat key mapping;
- `InputFrame`/`InputState` and the simulation-facing input command.

## Now: live key rebinding

The host already forwards key/char/scroll/drop events to the active generation
through `kEventGameInput` (723). The game owns the bind map.

Required proof: while the same process is running, rebind an action (for example
Jump from Space to F), use it, and observe the new binding with no EXE rebuild.
This is the first input hotness proof and must be in the journal.

Gameplay must not poll GLFW directly once the host event stream covers held
state. A direct device call is allowed only with a measured, documented reason,
and it must journal `game.input.path=direct` and appear in the hotness ledger.

## Later: normalized input

Extend `GameInputEventV1` into a normalized event:

```text
device: keyboard | mouse | gamepad | touch | pointer | replay | script
code:   normalized InputCode (not a GLFW constant)
value:  0..1 for axes, pressed/released/held for buttons
deviceId, sequence, timestamp, modifiers
```

Host backends translate OS events into this shape. The game never sees an OS
constant.

Adding a device/OS/controller is a host backend plus hot bindings, not a
gameplay change. This is the cross-platform direction and is expected to be
addable live once the hot-reload loop is stable.

## Performance exception

A direct device call is a deliberate cold exception. It requires:

- a measured reason in the hotness ledger;
- a journal path (`game.input.path=direct`);
- a plan to remove it or wrap it in a backend.

No silent direct calls.

## Expansion

- Input recording/replay becomes another input source.
- Touch/console/browser backends reuse the same normalized events.
- Binds remain game-owned data and can be migrated per account.
