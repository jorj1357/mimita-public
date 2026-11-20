# Log retention and storage-efficiency intake

- User request: “can you make it so logs oldest logs are auto deleted and a issue tracker for space efficiency not taking up a lot of space  goal is  0 wasted bytes every bytes has a purpose”
- Confirmed existing behavior: `devscripts/dev_loop/game_build.py::_prune_logs(root, 3)` prunes dated log folders before game-DLL builds and retries when disk space is low. It retains the three newest dated log folders.
- Read-only validation: `python devscripts/prune-builds.py --logs-only --dry-run --keep-log-days 3` reported one date folder kept, zero removed, and zero bytes freed.
- Tracker: created and triaged `issue-3f776ce60511`, linked user context, and opened task `storage-audit-v1` in the existing coordination issue system. The target is zero unaccounted bytes in explicitly scoped MiMITA-owned storage, with active and retained evidence protected.
- Existing related evidence: issue `issue-916633ab3313` records a build rejected at 1386 MB free against a 2048 MB minimum.
- No source behavior changed, no files were deleted, and no build or runtime proof was needed for this intake.
