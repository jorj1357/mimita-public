# Show every recent branch in git push helper

Date: 2026-11-09 16:12:35 EST  
UTC: 2026-11-09T21:12:35Z  
Branch: `checkpoint/pre-live-kernel-20261108`  
HEAD: `74fa982aabaf53f540d3df5f0201c0a5157140b0`

Result: PASS_WITH_HUMAN_REVIEW

## Pre-existing edits

- `devscripts/blender-ai` was already modified as a submodule when this task
  began. It was left untouched.

## Change

Updated `git-push-v3.py`, `main`, branch listing and selection (lines 46–58).
Previously, the script read all local branches ordered by descending
committer date, but displayed only `lines[:9]` and prompted for `1-9`. It now
displays and numbers every local branch in that order and derives the prompt's
upper bound from the list length. Enter still leaves the user on the current
branch; entering a number still selects the corresponding entry.

This uses Git's local branch committer dates as the available indicator of
recent updates. The helper does not query hosted push timestamps.

## Documents and focused review

- `docs/ROUTER.md`
- `docs/specs/dev-loop/agent-lanes-build-authority.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/skills/terminal-command-checker-v1.md`
- `docs/operations/task-completion/task-completion.md`
- Terminal command checker: PASS for scope; this is a repository utility, not
  an in-game terminal command.

## Validation

- Reviewed the source diff and current local branch ordering; branches are
  sorted by `--sort=-committerdate`.
- `git diff --check -- git-push-v3.py`: passed.
- No build or runtime validation applies to this Python repository utility.
- The helper was not interactively launched in this session.

## Human review still needed

Launch `git-push-v3.py` and confirm the full numbered list and numeric branch
selection in the console. The list reflects local branches only.
