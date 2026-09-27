# EXE Safety

# Mimita Engine

This is a C++17 OpenGL game engine.

## Live-development invariant

If `MiMITA.exe` is already running, it must remain running. Do not close,
restart, relink, replace, or unlock a running executable. Live iteration edits
hot sources (see `docs/architecture/live-development/live-development.md`) and
uses `python devscripts/live-build.py`, which never writes an EXE.

`python build_agent.py` (agents) and `python buildv3.py` (humans) are COLD BUILDS:
they relink an executable. They never relink an executable that is currently
running. `MIMITA_FORCE_COLD=1` is a loud, last-resort override for an intentional
cold build that must overwrite a running target; it is not part of the normal
loop. Never run `taskkill` on a running game process.

## Repository Workflow

When solving coding problems:

1. Search the repository first.
2. Identify only relevant files.
3. Read only relevant sections.
4. Reason about the issue.
5. Implement the smallest correct fix.

Never answer from assumptions before searching.

Use repository search before reasoning.

Rules:

* Read repository files directly.
* Answer coding questions concretely.
* Prefer minimal patches.
* Avoid unnecessary rewrites.
* Do not ask for clarification unless required to proceed.
* Fix problems directly.
* Keep solutions practical and shippable.

If there is a TODO comment in the file you are working on, and it is easy enough to do, just do it and continue rather than skipping it.

## Timestamped, non-colliding EXE output

Every cold build links a **new, uniquely named** executable in the project root:

```
mimita-YYYYMMDDTHHMMSS.exe
```

For example, a build at 5:37:58 PM on Sep 16 2026 produces:

```
mimita-20260916T173758.exe
```

The name is generated per build, so:

* Several agents and/or humans can each build and test their own changes without
  being blocked by a different running `mimita*.exe`.
* A running game never locks the next build's output.
* Each executable is compiled from the same current source tree, just at a
  different time; a build is a snapshot of that moment.

Build entries:

* Agents: `python build_agent.py`
* Humans: `python buildv3.py`

Both share the same build lock and pipeline (`build.py build-only`) and differ
only in who calls them. Optionally, set `MIMITA_EXE_NAME` to force a specific
output name (for example a fixed name for a deliberate test); if unset, the
timestamped name is used.

The previous "single canonical `mimita.exe`" rule is replaced by this
timestamped rule. Do not create feature-specific *test* executables (for example
`mimita-chat-test.exe`); use a normal timestamped build instead. The only fixed
name that still exists is the historical `mimita.exe` from older builds, which
may be running and must be left alone.

## Build result

## Cold-build record

Whenever an agent intentionally runs a cold build, it must append one
`Cold-build occurrence N` section to the dedicated record described in
`docs/regressions/README.md`. The entry is required even when the build
succeeds. Explain why the live build could not produce the requested result,
identify the exact cold source/owner or runtime boundary, state what the cold
build was needed to prove, and name the smallest change that would make this
work live-reloadable next time. Link the final session changelog and keep the
existing running-process safety rules unchanged.

After a build, read `build/build-result.json` (machine readable) or
`build/changelog.txt`. Both include the produced executable name and path:

```
=== BUILD CHANGELOG ===
Status: SUCCESS
Executable: mimita-20260916T173758.exe
```

`Status: SUCCESS` means a new executable was linked. `Status: NOTHING_CHANGED`
means the object files were already up to date; the referenced executable (if it
exists) is the current one. If a build is expected to change code but reports
`NOTHING_CHANGED`, that is still the current tree for the requested name.

## Running a build

Agents:

```
python build_agent.py
```

Humans:

```
python buildv3.py
```

Then launch the produced `mimita-<timestamp>.exe` from the project root so its
relative asset paths resolve (the executable-directory resolution in
`src/utils/path_utils.cpp` expects to sit beside `assets/`, `config/`, etc.).

Do not set `MIMITA_EXE_NAME` to overwrite a running executable and do not create
alternate test executables. Never kill a running `mimita*.exe`; a new timestamped
build is how you get your own executable to test.

After any build, check the status printed in the output and in
`build/changelog.txt`. If the status is `NOTHING_CHANGED` but you expected a
relink, delete the stale object for the file you changed (or the relevant
`build/obj-*/` object) and rebuild.
