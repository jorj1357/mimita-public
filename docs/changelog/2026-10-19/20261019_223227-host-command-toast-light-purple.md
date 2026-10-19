# Host-command result toast accent

- **Timestamp:** 2026-10-19T22:32:27Z (2026-10-19 18:32:27 EDT, America/New_York)
- **Branch/commit:** Not inspected.
- **Status:** `PASS_WITH_HUMAN_REVIEW`

## Result

Host-command result notifications now use a dedicated light-purple accent for
both title and message (`RGB 0.78, 0.62, 1.0`, approximately `#C79EFF`). Other
critical notifications remain red and important notifications remain yellow.
The received server-result path was exercised in the real client/server run;
human visual confirmation of the exact shade remains outstanding.

## Changed files and exact content

### `src/notifications/notifications.h`

- Added the `Accent` enum, stored accent on each notification, and declared
  `pushHostCommand` and the shared `pushWithAccent` helper.
- Before:

  ```cpp
  bool critical = false;      // red accent (title + message) for urgent notices
  bool important = false;     // yellow accent for successful, time-sensitive notices
  ```

- After:

  ```cpp
  enum class Accent { Default, Critical, Important, HostCommand };
  Accent accent = Accent::Default;
  void pushHostCommand(const std::string& title, const std::string& message,
                       uint64_t durationTicks = 0);
  ```

### `src/notifications/notifications.cpp`

- Lines 318-374: normal, critical, important, and host-command notifications
  share one insertion/typewriter path, avoiding the prior duplicated critical
  and important insertion logic.
- Lines 617-621: body text for `Accent::HostCommand` is assigned RGB
  `(0.78, 0.62, 1.0)`.
- Lines 761-764: title text receives the same host-command accent.
- Before, the title/body renderer handled only critical red and important
  yellow overrides; default text came from GUI config.
- After:

  ```cpp
  else if (notif.accent == Accent::HostCommand)
  {
      color.r = 0.78f;
      color.g = 0.62f;
      color.b = 1.0f;
  }
  ```

### `src/network/multiplayer-tick.cpp`

- Lines 2026-2034: the client routes both accepted and rejected server command
  result packets to `pushHostCommand`.
- Before:

  ```cpp
  NotificationSystem::instance().pushCritical(
      "Host command applied", status, 0);
  ```

- After:

  ```cpp
  NotificationSystem::instance().pushHostCommand(
      "Host command applied", status, 0);
  ```

  The rejected-result branch changes the same way, using title
  `Host command rejected`.

### `docs/changelog/2026-10-19/20261019_223227-host-command-toast-light-purple.md`

- New file; no prior text existed. It records the exact source change and
  validation evidence.

## Exact execution path

1. A server command request receives an acknowledgement in
   `src/network/server-packet-chat.cpp`, in `handleServerCommand()`'s `ack`
   lambda. That path writes `network.server-command.result-sent`.
2. The client receives `PACKET_SERVER_COMMAND_RESULT` in
   `src/network/multiplayer-tick.cpp` inside `mpTick()`, writes
   `network.server-command.result`, and calls `pushHostCommand()` for applied
   or rejected results.
3. `NotificationSystem::pushHostCommand()` in
   `src/notifications/notifications.cpp` calls `pushWithAccent()` and stores
   `Accent::HostCommand` on the notification.
4. `NotificationSystem::render()` applies the accent to the title; its
   `drawMessage()` applies it to the message body.

## Validation evidence

- Canonical dev-loop build request succeeded as build 2656. The published
  current artifact was subsequently build 2657 with the same source hash
  `db6daf0ad2a8bc6dc6e2bfe79fded46883462f4ce6a18cba6a67a073ca098395`.
- Ran the newly built executable with `--versioninfo`:
  `C:\mimita-v9\.dev\builds\2656\mimita.exe`; it reported
  `EVENTS_JSONL_PATH=logs/10-19-2026/20261019_183101/events.jsonl`.
- The dev loop launched a matching client/server pair on build 2657. The
  read-only `team_list` command was sent through the client terminal; server
  journal record `network.server-command.result-sent` shows `accepted=true`
  and status `1 = RED | 2 = BLUE`. The client journal record
  `network.server-command.result` shows the acknowledgement was received.
- Live journal inspected:
  `logs/2026-10-19/20261019_223206/events.jsonl` (run `20261019_223206`).
  The matching client result was at `seq=804`, PID 36532; the server result
  was at `seq=433`, PID 35644. These records prove the real response path
  reached `pushHostCommand`; they do not prove the pixels were visually
  inspected.
- `git diff --check -- src/notifications/notifications.h
  src/notifications/notifications.cpp src/network/multiplayer-tick.cpp`
  passed. No tests were added or run.

## Review still needed

Human visual review should confirm that `#C79EFF` reads as light purple and
remains legible in the running notification. The unrelated dev-loop profile
scenario also emitted a `startmelee` result during the same run; the
`team_list` probe itself is read-only. No unrelated files or processes were
stopped or edited.
