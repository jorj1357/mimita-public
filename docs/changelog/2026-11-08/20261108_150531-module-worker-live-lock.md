# Preserve live module build ownership

The module worker previously removed any per-module lock older than 120 seconds. A legitimate long compile could then overlap with another worker and allow competing writers for one generation. It now reads the owner PID and removes the lock only when Windows reports that process has exited. An unreadable lock is retained.

Static validation: Python syntax, module catalog audit, and diff whitespace checks passed. The dev loop remains active and is building newer source. Its current visible Payload/FunWorld3 run has zero NPCs, so this session does not claim the requested 19-NPC, 240 FPS, 60 Hz, hit agreement, or correction acceptance.

The larger extraction remains open: server NPC target selection and navigation, weapon claim policy, respawn behavior, and streaming/editor behavior still require distinct boundary work and real connected trials. This change preserves the build substrate during that work; it does not itself improve gameplay performance.
