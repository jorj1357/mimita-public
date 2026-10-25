# EXE Safety

# Mimita Engine

This is a C++17 OpenGL game engine.

## Current build authority

Submit builds through `python devscripts/dev-loop.py --request build
--wait-for-build`. The persistent dev-loop owns request ordering, build status,
artifact publication, and `[1]` launch selection. It runs the compiler pipeline
on one worker while continuing to accept launch and status requests. Pressing
`1` launches the latest successfully published artifact immediately, even if
it is older than the source currently building; build completion publishes a
new artifact but does not launch it automatically.

On Windows, `[1]` launches the selected numbered build through the fixed
`.dev/current/mimita.exe` path. The numbered directory remains the versioned,
immutable record; the fixed path is a hardlinked runtime bundle that DevLoop
updates after stopping the previous server. Firewall rules should target this
fixed executable path so later numbered builds keep the same app identity.
Status and event metadata must show both the fixed launch path and the numbered
artifact path/build identity.

`build_agent.py`, `buildv2.py`, and setup entry points are compatibility
wrappers that submit to this same dev-loop. `build.py build-only` and
`build_game_dll.py` are internal compiler steps. A checkout-wide OS lock makes
an accidental direct/overlapping compiler invocation fail clearly instead of
running a second build concurrently. Do not use `buildv3.py` or direct compiler
scripts as a separate live build path.

The build never terminates a running game to publish an artifact. Build
identity and process lifetime are separate: selecting a new artifact requires
an explicit `[1]` request, and a healthy old run remains alive after a build
failure.

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

Build entry:

```
python devscripts/dev-loop.py --request build --wait-for-build
```

Use `--build-mode release` when a release configuration is specifically
needed. The build output remains timestamped unless `MIMITA_EXE_NAME` is set
by an internal build workflow.

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

All contributors:

```
python devscripts/dev-loop.py --request build --wait-for-build
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
