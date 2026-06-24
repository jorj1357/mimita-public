---
name: ai-drift-prevention
description: Run BEFORE creating any new variable, function, or file. Searches existing code for duplicates and prevents state sprawl. Use when you are about to add new code, not for analysis of existing code.
mode: subagent
permission:
  edit: deny
  bash: allow
---

# AI Drift Prevention

You are about to add new code. Before writing ANY new variable, function, or file, follow these checks:

## Step 1: Check for duplicate state variables

If you are adding a new boolean/float/int member to `Player` (or any state struct):

```powershell
# Search for semantically similar existing variables
Select-String -Pattern "grounded|onGround|ground|landing" -Path src/entities/player.h
Select-String -Pattern "dash|dashAvailable|dashReady" -Path src/entities/player.h
Select-String -Pattern "jump|airJump|coyote|intent" -Path src/entities/player.h
```

If any existing variable represents the same concept, **do not create a new one**. Reuse the existing one.

## Step 2: Check for existing functions

If you are writing a new function:

```powershell
# Search for similar function names
Select-String -Pattern "doWalk|doJump|doDash|doFreeze|doGravity|doFriction" -Path src/physics/movement/*.cpp
Select-String -Pattern "^void |^bool |^int |^float " -Path src/physics/movement/*.cpp
```

If an existing function does the same job with different parameters, expand it. Do not write a parallel function.

## Step 3: Check for existing files

If you are creating a new `.cpp` or `.h` file:

Explain in your response which **existing file** cannot own the new behavior and why. If no existing file is suitable, state:

> "Creating new file because [existing files] cannot own [behavior] because [reason]."

## Step 4: Check for dead files

If you see a `.h` file with only comments and no corresponding `.cpp`, flag it for deletion.
If you see a function defined identically in multiple files, flag it for deduplication.

## Step 5: Ownership alignment

Every function belongs in the subsystem that owns its primary concern:
- Physics state → `Player` struct
- Physics logic → `src/physics/movement/`
- Rendering → `src/render/`
- Audio → `src/audio/`
- Terminal commands → subsystem's own file, NOT main.cpp

## Report format

After completing checks, report:

```
[DRIFT CHECK]
  New variable/function/file: <name>
  Existing duplicate found: <yes/no>
  If yes: <location of existing>
  New file justified: <yes/no>
  If yes: <reason>
  Action: <create new / expand existing / reject>
```
