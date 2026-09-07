// 2026-09-07 11:57 EST
/* purpose
* standardize AGENTS.md for readability: ISO 8601 timestamps, purpose comment block, mandatory changelog step, centralized logging enforcement
* align changelog path, file header, and debug-logging spec with time-and-formatting.md
* add "No Loose Debug Files" rule to prevent per-system log files bypassing centralized logging
* this file DOES NOT change game behavior, networking, or physics
* this file DOES NOT modify C++ source code
* this file DOES NOT rename historical changelog files
*/

# AGENTS.md Readability and ISO 8601 Standardization — 2026-09-07 11:57 EST

## Branch
develop/v2.0.1

## Time
2026-09-07T16:57:00Z

## Task
Make AGENTS.md more readable and enforce three documentation standards:
1. ISO 8601 timestamps and folder format across the entire repo (aligned with `docs/architecture/time-and-formatting/time-and-formatting.md`)
2. Replace the ambiguous purpose comment block placeholder with a real example
3. Add "Write changelog" as mandatory step 5 in Task Completion Requirements
4. Add "No Loose Debug Files" rule to enforce centralized Debug::log usage
5. Update debug-logging spec section 2 to use ISO 8601 as the primary format

## Pre-existing Changes
None — these files had no uncommitted edits before this session.

## Requested behavior
- AGENTS.md uses `yyyy-mm-dd` for changelog folders, ISO 8601 for timestamps
- Purpose comment block shows a real example instead of "fill in" placeholders
- Task Completion Requirements explicitly includes writing the changelog as step 5
- No loose .txt debug files — all output goes through centralized Debug::log
- Debug-logging spec uses ISO 8601 folder and filename format as the primary standard

## Specification alignment

- Current specification paths: `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/specs/debug-logging/debug-logging.md`, `docs/operations/task-completion/task-completion.md`
- Exact requirements: Time-and-formatting.md defines `yyyy-mm-dd` folders and `YYYY-MM-DDTHH:MM:SSZ` as the canonical standard. Debug-logging spec section 2 must match.
- Why the change follows the specification: AGENTS.md previously contradicted time-and-formatting.md by using `mm-dd-yyyy`. These edits align AGENTS.md with the authoritative standard.
- Conflicts or decisions: None. The time-and-formatting.md standard was already correct; AGENTS.md was the outlier.

## Exact implementation changes

### File: `AGENTS.md`

- **Lines 16-22 (changelog path and timestamp instruction):**
  - Old: `docs/changelog/mm-dd-yyyy/` with "time in EST"
  - New: `docs/changelog/yyyy-mm-dd/` with "timestamp in ISO 8601 UTC (`YYYY-MM-DDTHH:MM:SSZ`)"
  - Added emphasis: "The changelog must be as detailed as possible — never skip or abbreviate it."
  - Reason: Align with time-and-formatting.md standard. Remove ambiguity about timestamp format.

- **Lines 104-127 (purpose comment block):**
  - Old: `// mm dd yyyy, hh mm` with placeholder text "fill in purpose of file", "fill in 2nd line"
  - New: `// YYYY-MM-DD HH:MM EST` with descriptive labels ("one-line summary of what this file does") and a complete ragdoll example
  - Reason: The old template was confusing — "fill in" instructions looked like they might be the actual output. A real example makes the format immediately clear.

- **Lines 725-755 (new "No Loose Debug Files" subsection after Logging Rules):**
  - Added entirely new section forbidding `fopen`/`fprintf`/`printf` to loose `.txt` files in `logs/`
  - Includes forbidden pattern (raw fopen) and correct pattern (Debug::log)
  - Notes that existing loose files (e.g., `ragdoll_diagnostic.txt`) are legacy and should migrate when touched
  - Reason: The ragdoll system writes to `logs/ragdoll_diagnostic.txt` via raw `fopen` (ragdoll.cpp:38), bypassing the centralized logging system. This rule prevents new systems from doing the same.

- **Lines 1059-1067 (Task Completion Requirements):**
  - Old: 4 steps + "Final Validation" + 3 more steps (7 total, no changelog step)
  - New: 5 steps (added step 5 "Write changelog") + "Final Validation" + 3 more steps (8 total)
  - Step 5 includes full specification: path format, filename format, mandatory requirement, detail level
  - Reason: The changelog write was documented at the top of AGENTS.md and in task-completion.md, but not in the task completion steps themselves. Agents could miss it.

- **Lines 1104-1112 (Task Completion Hook example):**
  - Old: `python build_agent.py` then `python devscripts/agent_task_complete.py`
  - New: Added `:: write changelog: docs/changelog/2026-09-07/20260907_143000-fix-duel-replay.md` between build and completion hook
  - Reason: Reinforces that changelog writing is part of the completion flow, not optional.

### File: `docs/changelog/TEMPLATE.md`

- **Line 1 (header comment):**
  - Old: `// 09 06 2026, 12 00`
  - New: `// 2026-09-06 12:00 EST`
  - Reason: Align template with ISO 8601 format.

### File: `docs/specs/debug-logging/debug-logging.md`

- **Lines 46-83 (section 2: Log folder and file output):**
  - Old: Primary format was `mm-dd-yyyy` folders and `mmddyyyy_hhmmss` filenames, with ISO 8601 mentioned as "new generated logs" in passing
  - New: Primary format is `yyyy-mm-dd` folders and `yyyymmdd_hhmmss` filenames. Old format explicitly marked as "Historical format (pre-2026-09-06)" and noted as invalid for new logs.
  - Reason: Align with time-and-formatting.md. Remove contradiction where two different formats were presented as co-equal.

## Diagnostics
- Not applicable — this is a documentation-only change, no game behavior changed.

## Validation
- Focused skill paths and results: N/A (documentation-only change)
- Tests and exact commands: Manual review of all edited sections
- Build status: N/A (no code changed)
- Runtime or hot-reload evidence: N/A
- Output files: Verified all 5 edited sections in AGENTS.md, 1 in TEMPLATE.md, 1 in debug-logging.md

## Measured evidence
- Before: AGENTS.md used `mm-dd-yyyy` for changelog path, ambiguous purpose block, no changelog step in task completion, no rule against loose debug files
- After: All paths use `yyyy-mm-dd`, purpose block has real example, changelog is mandatory step 5, loose debug files are forbidden
- Files changed: 3 (AGENTS.md, TEMPLATE.md, debug-logging.md)

## Regression review
- Regression entry appended: no
- Why this is not a confirmed regression: This is a documentation standardization, not a behavior change. No gameplay, networking, or physics was modified.
- Related regression paths: none

## Human acceptance
- Visual review: needed — verify AGENTS.md reads clearly after edits
- Gameplay review: not applicable
- Multiplayer review: not applicable
- Still unverified: Whether existing agents/sessions will follow the new changelog format consistently

## Known follow-up items (not in scope for this session)
- `src/debug/log-manager.cpp:34` still uses `%m-%d-%Y` for runtime log folder names. Code change needed to match the ISO 8601 standard.
- `src/ragdoll/ragdoll.cpp:38` uses `fopen("logs/ragdoll_diagnostic.txt")` — a loose debug file that violates the new "No Loose Debug Files" rule. Should migrate to `Debug::log(Debug::Category::Ragdoll, ...)` when the ragdoll system is next touched.
