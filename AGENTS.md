# Mimita Engine

This is a C++17 OpenGL game engine.

# Repository workflow

When solving coding problems:

1. First grep/search the repository.
2. Identify only the relevant files.
3. Read only relevant sections.
4. Then reason about the issue.
5. Then propose patches.

Never answer from assumptions before searching.

Use repo search before reasoning.

Behavior rules:
- Read repository files directly.
- Answer coding questions concretely.
- Do not discuss planning systems.
- Do not discuss permissions.
- Do not ask for clarification unless necessary.
- Focus on fixing problems directly.
- Prefer minimal patches.
- Avoid meta-discussion.

Do not narrate reasoning.
Do not output thought processes.
Answer directly.

# Architecture direction (target, not strict)

- Prefer small files. Ideally every file is 100 lines or less.
- One clear responsibility per file. Ideally one file exposes one main function.
  Example: gravity file exposes doGravity(), packet send file exposes sendPacket().
- Avoid huge files that mix unrelated systems.
- Main files should orchestrate, not contain feature logic.
- First line of each new file should be date created.
- Second line should be file path.
- Third line and following should explain purpose of file.
- Make code easy to reason about for performance, bugs, and AI agents. Also humans.

# Terminal commands

- Every game action should ideally be callable through a terminal command.
- The whole game should eventually be playable/testable through terminal commands.
- UI, hotkeys, and gameplay input should reuse terminal/action functions where practical.
- Terminal command registration should be moved out of main.cpp over time.
  main.cpp should only call registration functions like:
    registerReplayCommands(); registerWeaponCommands(); etc.
  Feature files should expose registration functions that main.cpp calls.