# Visible game window regression record

- Added a dev-loop regression record separating window visibility, foreground activation, and taskbar presence.
- Recorded the confirmed `SW_SHOWNA`/foreground-restore behavior and the `prelaunch_windows` uninitialized error when reusing a server.
- Documented the partial launcher changes and their runtime evidence. Foreground activation did not succeed in the captured run, and taskbar presence remains unverified.
- Indexed the record as Entry 24 in `docs/regressions/dev-loop/README.md`.
