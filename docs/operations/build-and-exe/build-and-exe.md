# EXE Safety

# VPS Branch Verification and Deployment

Before pulling code onto the VPS:

1. Identify the most recently updated candidate branch from the authoritative Git remote.
2. Confirm its branch name, latest commit, commit date, and intended task with the user before deployment.
3. Do not assume `develop/v2.0.1` or any version branch is current; those branches may be stale.
4. Pull only the confirmed branch with `git pull --ff-only origin <confirmed-branch>`.
5. Preserve and report any pre-existing untracked or local VPS files; never delete or overwrite them as part of a pull.
6. Verify the deployed commit and restart only the relevant existing service after a successful pull.

Never launch `mimita.exe` without `--server` or `--timeout <secs>`. Without these flags the game opens a full graphics window and stays open indefinitely (it won't automatically exit). If you need to test server behavior, always use `--server --timeout 30 --no-coordinator` or similar so the process self-terminates.

For build purposes, human or AI agents are authorized to terminate existing `mimita.exe` processes at any time, until this instruction is changed. This authorization applies only to releasing the executable lock so the updated build can be produced and tested.

# Mimita Engine

This is a C++17 OpenGL game engine.

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

Do not output chain-of-thought.
Do not narrate internal reasoning.

When building or testing the EXE, use build_agent.py instead of build.py, because build.py opens the EXE on the computer and may falsely appear to error when it has not.

## Single EXE Output

All development builds must use the single canonical output:

```
C:\mimita-priv-v8\mimita.exe
```

Run:

```
python build_agent.py
```

Do not set `MIMITA_EXE_NAME` and do not create alternate development
executables such as `mimita-chat-test.exe`, `mimita-duel-handshake-test.exe`,
or feature-specific test executables. Focused tests must use the canonical
`mimita.exe` or a non-EXE test harness. Before building, close any running
`mimita.exe`; never kill a possibly user-owned process to release the file.

After any build_agent.py invocation, check the build result status printed in the output:

```
=== BUILD CHANGELOG ===
Status: SUCCESS
```

or:

```
Status: NOTHING_CHANGED
```

If the status is NOTHING_CHANGED and you expected changes, the human may have built first. Read `build/changelog.txt` for the full build log. The changelog always reflects the most recent build_agent.py run.

If you get NOTHING_CHANGED but changed source files, the human may have built first. Read `build/changelog.txt` for the full build log. The changelog always reflects the most recent build_agent.py run.

If you get NOTHING_CHANGED but changed source files, force a rebuild by deleting the EXE:
```powershell
Remove-Item -Force "mimita.exe" -ErrorAction SilentlyContinue; python build_agent.py
```

The changelog at `build/changelog.txt` is written after every `build_agent.py` invocation. Its first three lines always show:

```
=== BUILD CHANGELOG ===
Time: YYYY-MM-DD HH:MM:SS
Status: SUCCESS|NOTHING_CHANGED|FAILED
```

Always check this status after building. If the human built between your source edits and your build_agent.py call, you will see NOTHING_CHANGED even though your edits should trigger a rebuild. Delete mimita.exe and rebuild in that case.

