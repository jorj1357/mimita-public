---
title: "Mimita V1 Beginner Guide"
description: "Everything you need to know to start playing Mimita — movement, weapons, duels, replay clips, and beginner tips."
date: "2026-06-12"
author: "admin"
tags: ["guide", "beginner", "getting-started"]
published: true
---

## What is Mimita?

Mimita is an experimental first-person shooter built around **momentum-based movement**, **one-shot-kill weapons**, and **fast-paced duels**. It is developed by one person and is not trying to be a polished AAA game — it is raw, weird, and built for fun.

Right now Mimita is in **early V1**. Things change fast. If something breaks, check back for an update.

---

## How to Start Playing

1. Go to the [download page](/download) and grab the latest build.
2. Extract the zip and run `mimita.exe`.
3. The game starts in a sandbox mode. You can move around, shoot NPCs, and explore.
4. Open the terminal with the **grave / backtick key** (`). From there you can type commands.

### Useful first commands
- `help` — list all commands
- `npc spawn` — spawn a bot to practice on
- `replay_test` — record a test replay (runs automatically for 300 ticks)
- `duel start` — start a duel match

---

## Basic Movement

Mimita movement is inspired by shooters that reward speed and air control.

| Action | Default Key |
|--------|-------------|
| Move | WASD |
| Jump | Space |
| Dash | Shift (while moving) |
| Freeze | F (hold to freeze in air) |
| Down Dash | Ctrl (while airborne) |

### Important movement techniques

**Dash** gives you a burst of speed in the direction you are moving. It resets when you touch the ground. You can chain dashes with jumps to maintain speed.

**Air jump** lets you jump once while airborne. Press space after leaving a ledge.

**Freeze** stops all momentum mid-air. Useful for aiming or avoiding a shot.

**Ground return** pulls you back to the ground if you are knocked too far up.

Movement is experimental. If you find a way to break the speed limit, that is a feature, not a bug.

---

## Basic Weapons

### Revolver (Slot 1)
- 6 shots per magazine
- Semi-automatic
- High damage, accurate
- Good for mid-range fights

### Godball (Slot 2)
- Projectile weapon
- Bounces off walls
- Slower but very lethal
- Requires leading your target

### Shotgun (Slot 3)
- 2 shots per magazine
- Devastating up close
- Spread makes it less reliable at range
- One shot can kill if all pellets hit

Switch weapons with the **1, 2, 3** keys or the mouse wheel.

---

## Duels

Duels are 1v1 (or player-vs-NPC) matches. Start one with:

```
duel start
```

Each duel has a **kill limit** and a **time limit**. First to reach the kill count wins.

During the duel, you respawn automatically after dying. The match ends when someone wins or the timer runs out.

---

## Replay Clips

Mimita automatically records the last 60 seconds of gameplay in a ring buffer.

When you get a kill, a clip is automatically saved 5 seconds before and 3 seconds after the kill.

### Manual clip save
Press **F8** during gameplay to save the last kill as a clip.

### Browse clips
Press **F9** to open the replay browser. From there you can play, rename, or delete clips.

### Replay controls
Once a clip is playing:

| Key | Action |
|-----|--------|
| Space | Pause / Resume |
| Arrow keys | Step through frames |
| R | Restart |
| Mouse | Look around (freecam mode) |

Change camera modes with:
- `replay_camera fp` — first person (killer POV)
- `replay_camera victim` — victim POV
- `replay_camera orbit` — orbit camera
- `replay_camera freecam` — free camera

### Timeline
During playback, a timeline bar appears at the bottom of the screen showing event markers (kills, dashes, jumps, weapon switches). Click or seek to jump to any moment.

---

## Why the Game is Experimental

Mimita is built by a solo developer who is learning as they go. The codebase is messy, the physics occasionally hallucinate, and features appear and disappear without warning.

The website is also a work in progress. Some pages do not exist yet (looking at you, feedback page). If you encounter bugs or missing features, that is normal.

The project exists because it is fun to make and fun to play. If that resonates with you, you are in the right place.

---

## Beginner Tips

1. **Move before you shoot.** Standing still gets you killed. Dash, jump, and strafe.
2. **Learn the revolver first.** It is the most forgiving weapon and teaches you aim.
3. **Use freeze to line up shots.** Hold F while mid-air to stop and aim.
4. **Watch your replays.** Press F9 after a match to see what you did wrong.
5. **The dash resets on ground contact.** Use this to maintain speed across the map.
6. **Experiment.** There is no meta. Find what works for you.

---

## Getting Help

- Open the in-game terminal and type `help`.
- Visit the [socials page](/socials) for community links.
- Check back here for more guides as they are written.
