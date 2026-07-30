MiMITA In-Game Chat, Tips, Reports, Profile Hover, and Notifications Full Specification
Version: July 30, 2026
Status: Implementation target
Target repository: C:\important\mimita-priv-v8
________________


1. End Goal
MiMITA has one complete social text-chat system that works:
* In solo games.
* In player-hosted games.
* On dedicated servers.
* For the host.
* For remote clients.
* In replays.
* With registered users.
* With guests.
* With the existing networking architecture.
There is no separate local-chat implementation.
The complete flow is:
player presses /
        ↓
chat input captures keyboard
        ↓
client sends ChatRequest
        ↓
server validates message
        ↓
server creates ChatMessageEvent
        ↓
server reliably broadcasts event
        ↓
every client displays the same message
The chat is:
* Global.
* Text only for the first version.
* Independent of player distance.
* Server authoritative.
* UTF-8 and Unicode compatible.
* Scrollable.
* Connected to player profiles.
* Connected to server moderation and reporting.
* Visible in replays.
* Controlled through console commands.
* Built from reusable, JSON-editable GUI components.
Voice chat will be implemented later as a separate system supporting global and proximity modes.
________________


2. Networking Rules
Chat uses the existing generic networking architecture.
Chat messages must use the reliable ordered transport channel because every accepted message must arrive and messages must remain in their correct order.
Do not create separate systems such as:
LocalChat
OnlineChat
HostChat
DedicatedServerChat
ReplayChat
There is only:
ChatSystem
The server owns:
* Whether a message was accepted.
* The authoritative message ID.
* The authoritative server tick.
* The UTC timestamp.
* Message ordering.
* Rate limiting.
* Operator permissions.
* Server messages.
* Report creation.
* Typing-state replication.
* Maximum message length.
* Message sanitization and validation.
The client owns:
* What the player is currently typing.
* Opening and closing the chat input.
* Rendering received messages.
* Scrolling.
* Mouse unlock and UI interaction.
* Hovering over usernames.
* Showing cached profile information.
* Immediate local typing presentation.
The host is still a client and uses the same request path as every remote client.
________________


3. Chat Request and Event Structures
Conceptual structures:
using ChatMessageId = uint64_t;
using ReportId = uint64_t;


enum class ChatSenderType : uint8_t
{
    Player,
    Server
};


enum class ChatChannel : uint8_t
{
    Global
};


struct ChatRequest
{
    uint32_t requestId;
    uint64_t clientSimulationTick;
    std::string utf8Message;
};


struct ChatMessageEvent
{
    ChatMessageId messageId;


    uint64_t serverTick;
    int64_t utcUnixMilliseconds;


    ChatSenderType senderType;
    EntityId senderEntityId;
    AccountId senderAccountId;


    ChatChannel channel;


    std::string utf8Message;
};
Do not copy avatar data, MMR, join date, profile URL, or full role presentation into every chat message.
Messages identify their sender.
The client then obtains presentation information from the player/profile system:
ChatMessageEvent
        ↓
senderAccountId / senderEntityId
        ↓
Player/Profile Cache
        ↓
username
role
avatar
MMR
join date
profile URL
username visual style
The authoritative message event may retain an immutable display-name snapshot if needed for replays or moderation evidence, but live profile data should not be duplicated into every network packet.
________________


4. Message Display Format
Default display format:
98442 jorj1357: hi message
98452 jorj1357: this also message
The first number is the authoritative server tick at which the message was accepted.
The tick is simple, deterministic, useful for debugging, and easy to connect to replay and moderation evidence.
Hovering over the tick shows:
Server tick: 98442
UTC: 2026-07-30 15:24:18.442 UTC
A configuration option may later display both directly:
98442 | 15:24:18 UTC | jorj1357: hi
The server stores UTC using Unix time or another timezone-independent representation.
Do not store local timezone strings in the authoritative message.
All users see the same UTC time.
Clients may later optionally show a local-time conversion, but moderation records, replay records, logs, and server data always use UTC.
________________


5. Message Length and Encoding
Maximum message length:
256 Unicode code points
The implementation must distinguish between:
* Bytes.
* Unicode code points.
* Visible grapheme clusters.
Do not truncate in the middle of a UTF-8 sequence.
At minimum:
1. Validate that the input is valid UTF-8.
2. Limit it safely without corrupting multibyte characters.
3. Reject embedded null characters and invalid control sequences.
4. Permit international languages and emojis.
Supported content includes:
* English.
* Spanish.
* Arabic.
* Chinese.
* Japanese.
* Korean.
* Cyrillic.
* Accented text.
* Emojis.
* Other valid Unicode text.
Font fallback must be used when MingLiU does not contain a requested glyph.
Default text font:
MingLiU
Fallback fonts must render missing Unicode characters rather than displaying empty boxes whenever possible.
________________


6. Opening and Closing Chat
Pressing:
/
from gameplay opens the chat input.
This must work from anywhere during normal gameplay unless another higher-priority text input owns keyboard focus.
When chat opens:
* Keyboard input is captured by the chat textbox.
* Gameplay movement and weapon input are suspended or consumed as appropriate.
* The mouse unlock system activates.
* A clear blinking typing cursor appears.
* A local typing indicator appears in the chat.
* A replicated typing-state request is sent to the server.
Pressing Enter:
* Attempts to send the message.
* Clears keyboard capture.
* Ends typing mode.
* Relocks the mouse according to the prior gameplay state.
* Sends typing state false.
Pressing Escape:
* Does not send the message.
* Clears keyboard capture.
* Ends typing mode.
* Relocks the mouse according to the prior gameplay state.
* Sends typing state false.
Pressing / should not insert a slash into the textbox unless configured to do so.
The existing console command:
chat <message>
must continue using the same underlying request function as the GUI.
There must not be one implementation for console chat and another implementation for GUI chat.
Both call:
requestSendChatMessage(message)

________________
7. Empty Messages
Before sending, trim whitespace for validation.
The following must not be broadcast:
""
" "
"       "
"\t"
"\n"
When an empty message is attempted, the requesting player receives:
[server]: u cant send nothing, silly!
This message may be sent only to that player rather than to the entire server.
The original empty message is not added to global history.
________________


8. Chat Rate Limiting
The server performs all authoritative rate-limit checks.
8.1 Minimum spacing
Default minimum spacing:
30 server ticks between accepted messages
At 60 simulation ticks per second:
30 ticks = 0.5 seconds
A message sent before the minimum interval expires is rejected.
The sender receives:
[server]: too fast! u have 17 ticks left before u can chat again
The remaining tick count is authoritative and calculated by the server.
8.2 Burst limit
Maximum burst:
5 accepted messages inside the configured burst window
Example:
tick 11: accepted
tick 12: accepted
tick 13: accepted
tick 15: accepted
tick 16: accepted
tick 17: blocked
Because this example conflicts with the normal 30-tick spacing rule, the implementation needs two configurable modes:
{
  "minimumTicksBetweenMessages": 30,
  "burstMessageLimit": 5,
  "burstWindowTicks": 300,
  "burstLockoutTicks": 300
}
Recommended behavior:
* The 30-tick rule limits continuous message speed.
* The burst window limits attempts or specially permitted rapid messages.
* Server/operator messages may have a separate policy.
* Rejected attempts do not broadcast.
* Repeated rejected attempts may extend the lockout only if configured.
When burst-limited:
[server]: too fast! u have 284 ticks left before u can chat again
All values must be configurable rather than embedded throughout the code.
________________


9. Chat History
Each client stores the latest:
100 accepted messages
Ordering:
oldest at top
newest at bottom
When message 101 arrives:
* Remove the oldest retained message.
* Append the new message to the bottom.
Messages appear immediately after receipt.
No entrance animation is required.
Chat tips and server messages count toward the 100-message limit.
________________


10. Scrolling
The chat window is scrollable immediately.
Mouse-wheel input over the chat window scrolls the message history.
When the player is viewing the bottom:
* New messages keep the view pinned to the newest message.
When the player has scrolled upward:
* Do not force the view back to the bottom.
* Continue receiving and retaining messages.
* Show a small indicator such as:
3 new messages
Clicking the indicator returns to the newest message.
The mouse must unlock when the player intentionally interacts with chat.
________________


11. Mouse Unlock System
The chat implementation includes a functioning mouse unlock system now.
Required states:
enum class MouseCaptureMode
{
    GameplayLocked,
    GuiUnlocked,
    TemporaryGuiInteraction
};
The system must:
* Remember the prior mouse state.
* Release cursor lock when chat input opens.
* Release cursor lock when the player activates chat interaction.
* Allow hover detection.
* Allow clicking usernames.
* Allow scrolling.
* Allow clicking profile links.
* Restore gameplay mouse lock after Enter or Escape when appropriate.
* Avoid leaving the mouse permanently unlocked after UI closure.
* Avoid firing a weapon when clicking chat UI.
* Avoid moving the camera while interacting with chat UI.
A configurable interaction key may later temporarily unlock the mouse while keeping chat closed.
For now, opening chat with / is enough to enable full interaction.
________________


12. Interactive Usernames
Usernames are interactive in this implementation.
Each rendered username has a hitbox.
Hovering over a username opens a profile-preview card.
The preview is positioned so it remains inside the visible screen.
The preview must not cover the username being inspected when avoidable.
The preview follows the reusable GUI system and is JSON editable.
________________


13. Registered-User Profile Preview
For a registered account, show:
Avatar preview


Username
Primary role
Join date
Current MMR
Basic match stats
mimita.fun profile link
Example:
jorj1357
VIP


Joined:
2026-04-19


MMR:
1482


Wins:
53


Profile:
https://mimita.fun/profile/jorj1357
The exact URL format must use the account’s canonical profile identifier rather than trusting the visible username.
Clicking the profile link opens the user’s MiMITA profile using the existing safe external-link system.
Profile information may initially be incomplete.
Missing information displays:
Not available
Do not block chat rendering while waiting for profile data.
Render the username immediately, then populate the hover card when profile information becomes available.
________________


14. Guest Profile Preview
For a guest, show:
Guest username


This is a guest.
They do not have a MiMITA profile.


Session ID:
<safe shortened ID>
Do not expose IP addresses, authentication tokens, machine IDs, or private network identifiers.
A guest can still be:
* Muted.
* Reported.
* Listed through listusers.
* Identified in moderation evidence through an internal server/session identifier.
________________


15. Username Role Priority and Styling
Role priority from highest to lowest:
Owner
Admin
Host
Moderator / Operator for the current server
Ultra VIP
Super VIP
VIP
Normal user
When a user has multiple roles, the highest role determines the default username style unless an explicit allowed cosmetic override exists.
Owner
Full black
Admin
Dark red
Host
Host uses a distinct JSON-configured style that cannot be confused with owner, admin, or moderator.
Moderator / Operator
Red
This role is server-specific.
Ultra VIP
May use:
* Rainbow text.
* Per-letter colors.
* Animated color effects.
* Other approved username effects.
Super VIP
May select any solid color except reserved role colors.
Reserved colors include:
* Owner black.
* Admin dark red.
* Moderator red.
* VIP turquoise.
* Host reserved style.
* Server-message reserved style.
VIP
Turquoise
Normal user
White
All role styles must come from hot-reloadable GUI or role-presentation configuration.
________________


16. Typing Indicator in Chat
When the local player is typing, show an obvious blinking indicator:
typing...
or:
jorj1357 is typing...
Other clients also see that the player is typing.
Typing state uses a lightweight network event:
struct PlayerTypingStateRequest
{
    bool isTyping;
    uint32_t sequence;
};


struct PlayerTypingStateEvent
{
    EntityId playerId;
    bool isTyping;
    uint64_t serverTick;
};
Typing-state requirements:
* Send true when typing begins.
* Send false when Enter is pressed.
* Send false when Escape is pressed.
* Clear typing state on death if chat is closed.
* Clear typing state on disconnect.
* Clear stale typing state automatically after a timeout.
* Deduplicate repeated state packets.
* Do not send a packet every tick.
* Send only when state changes, plus an optional low-frequency heartbeat.
________________


17. World Typing Indicator
When a player is typing, a visible bubble appears above their avatar.
Example:
[ typing... ]
or:
💬 ...
The world indicator:
* Is visible to other players.
* Is attached to the player’s replicated entity.
* Uses the replicated typing-state event.
* Does not reveal the draft message.
* Is hidden when typing ends.
* Is hidden after the typing timeout.
* Is cleared on disconnect.
* Is configurable through JSON.
* Does not depend on render framerate for blink timing.
The local player may also see their own indicator if configured.
________________


18. Chat Window Layout
Default reference resolution:
1024 × 768
Default approximate size:
Width: 25% of screen
Height: 25% of screen
Anchor: bottom-left
Nothing important is hardcoded to 1024×768.
The layout must scale to other resolutions and aspect ratios.
The GUI specification requires reusable, JSON-editable components, including editable position, size, text, color, effects, and per-letter presentation. fileciteturn2file0L26-L58
Example conceptual config:
{
  "chatWindow": {
    "anchor": "bottom_left",
    "widthNormalized": 0.25,
    "heightNormalized": 0.25,
    "offsetPixels": [12, -12],
    "maximumBackgroundOpacity": 0.5,
    "font": "MingLiU",
    "maximumMessages": 100,
    "showTickNumber": true,
    "showUtcTimeOnHover": true
  }
}

________________
19. Chat Window Visibility and Fade
Command:
chatwindow 0
chatwindow 1
Default:
chatwindow 1
When disabled:
* Chat messages are still received internally if needed.
* The visible chat window is hidden.
* Player messages are hidden.
* Tips are hidden.
* Server messages are hidden.
* Replay and moderation capture remain unaffected.
Server messages cannot be disabled independently.
Fade behavior
When a message arrives:
background opacity = 50%
For the next:
600 UI ticks
the chat remains at 50% maximum background opacity.
Then over:
300 UI ticks
the entire chat window fades linearly from:
50% → 0%
Timeline:
message at tick 0
ticks 0–600: 50%
ticks 601–900: linear fade
tick 900: 0%
The whole box fades together.
When the player opens chat, hovers it, scrolls it, or interacts with it:
* Restore visibility.
* Pause or reset the fade as configured.
The message text may have separately configurable opacity, but the maximum background opacity is 50%.
________________


20. GUI Tick System
GUI timing must not depend on render framerate.
Create a lightweight client-side UI tick clock.
Default:
60 UI ticks per second
The UI clock continues while:
* Main menu is open.
* Settings are open.
* Avatar editor is open.
* Server browser is open.
* In-game HUD is active.
* The window remains running but rendering performance changes.
The UI clock does not need to run as heavyweight gameplay simulation.
It is used for deterministic UI timing such as:
* Chat fade.
* Notification duration.
* Tip scheduling presentation.
* Blinking cursors.
* Typing animation.
* Menu effects.
* Temporary notification mute durations.
Use accumulated real monotonic time:
while (uiAccumulator >= uiFixedDeltaTime)
{
    stepUiTick();
    uiAccumulator -= uiFixedDeltaTime;
}
Do not tie UI timing to rendered frames.
________________


21. Server Tips Format
Tips appear through the same chat-message pipeline as server messages.
Format:
[server] Tip #25: aim at the ground, shoot, and let go of WASD to go far with rocket launcher
Each tip has a stable numeric ID.
Example tips.json:
{
  "tips": [
    {
      "id": 25,
      "text": "aim at the ground, shoot, and let go of WASD to go far with rocket launcher"
    }
  ]
}
File:
C:\important\mimita-priv-v8\config\tips.json
The website must read the same canonical tip data or receive the same generated data during deployment.
Do not maintain one unrelated game tip list and another unrelated website tip list.
________________


22. Tips Configuration
Create:
C:\important\mimita-priv-v8\config\tipsconfig.json
This file is hot reloadable.
Recommended format:
{
  "enabledByDefault": true,
  "minimumTicksBetweenTips": 1800,
  "maximumTicksBetweenTips": 3600,
  "showStartupDisableMessage": true,
  "startupMessage": "run chattips 0 in console to disable chat tips"
}
Defaults at 60 ticks per second:
1800 ticks = 30 seconds
3600 ticks = 60 seconds
Your rough values described 1800 as 15 seconds and 3600 as 30 seconds, but at 60 Hz they equal 30 and 60 seconds. To get 15–30 seconds, use:
{
  "minimumTicksBetweenTips": 900,
  "maximumTicksBetweenTips": 1800
}
The parser should permit large positive integer values.
Use a 64-bit integer:
uint64_t
Validation:
minimum >= 1
maximum >= minimum
If the values are invalid:
* Keep the last known valid config.
* Log the parse or validation error.
* Do not crash.
* Do not silently substitute unrelated values.
________________


23. Tip Scheduling Performance
Tip scheduling must not create a main-thread performance problem.
Do not repeatedly scan the JSON file every gameplay tick.
Correct flow:
startup or hot reload
        ↓
parse tips.json once
        ↓
parse tipsconfig.json once
        ↓
store validated configuration
        ↓
choose next scheduled UI/server tick
        ↓
each tick perform one integer comparison
        ↓
when reached, select a random tip and schedule the next tick
Conceptually:
if (currentTick >= nextTipTick)
{
    sendRandomTip();
    nextTipTick = currentTick + randomRange(minTicks, maxTicks);
}
This is effectively negligible work.
Hot reload should occur through:
* Existing file-watcher infrastructure, or
* A low-frequency modification-time check.
Do not parse both JSON files 60 times per second.
________________


24. Chat Tips Commands
Default:
chattips 1
Commands:
chattips 0
chattips 1
This preference is local to the client unless server policy explicitly requires otherwise.
Disabling chat tips:
* Hides automated tips for that client.
* Does not disable manual server messages.
* Does not disable the main-menu tip box unless shared behavior is explicitly configured.
* Does not stop the authoritative server from generating tips for other users.
________________


25. Floating Tip Box
The tip box appears across all major GUI screens, including:
* Main menu.
* Play menu.
* Settings.
* Avatar editor.
* Replays.
* Server browser.
* Other compatible GUI screens.
It is its own reusable floating GUI component.
It displays one tip from tips.json.
Controls:
NEW TIP
X
NEW TIP:
* Immediately chooses another random tip.
* Avoids returning the same tip twice in a row when multiple tips exist.
X:
* Hides the floating tip box.
* Creates a popup notification explaining what happened.
Example notification:
Tip box hidden.
Use tips 1 in the console to show it again.
Commands:
tips 0
tips 1
These control the floating tip box.
They are separate from:
chattips 0
chattips 1
which control automated tips appearing in chat.
________________


26. Server Messages
Authorized users may run:
servermessage <message>
Authorized roles:
* Owner.
* Admin.
* Current server host.
* Current server operator/moderator.
The server validates permissions.
Unauthorized users receive:
[server]: you do not have permission to use servermessage
The message is broadcast as:
[server]: welcome to the server
It uses the same ChatMessageEvent system.
It is not inserted locally without server confirmation.
________________


27. User Listing
Command:
listusers
Example:
1. jorj1357
2. IllIIlIIl
3. person3
4. abcdefg123
The numeric indexes are stable for the current command session or console interaction.
They must not silently change while a multi-step command is asking the player to choose a user.
Internally, selection resolves to a stable account ID, entity ID, or session ID.
Never use the temporary displayed number as the permanent identity in reports or moderation storage.
________________


28. Muting
Commands:
chatmute <username>
chatmute <listusers index>
Example:
chatmute 2
When a muted user sends a message, the muting client sees:
[Muted message]
Recommended behavior:
* The original message is still accepted and delivered by the server.
* Each client applies its own local mute preference.
* Other users who have not muted the sender still see the message.
* The muted message placeholder remains connected to the sender and timestamp.
* The placeholder may be clickable to reveal the individual message temporarily.
This avoids making one user’s personal mute globally censor another player.
Server-level moderation silence should be a separate permission action.
________________


29. Reporting Command Flow
Command:
reportuser
Interactive console flow:
> reportuser


Username or ID?


1. jorj1357
2. IllIIlIIl
3. person3
4. abcdefg123


> 3


Reason for report?


> repeatedly spamming threats in chat


Report submitted.
Report ID: 849201
The selection accepts:
* Current list index.
* Exact username.
* Stable account ID when available.
The report must resolve to a stable target before continuing.
If the user disconnects after selection, the report can still be completed using the captured stable identity.
The reporter may cancel with:
cancel
or Escape if supported by the console UI.
________________


30. Report Request Structure
Conceptual structure:
struct SubmitUserReportRequest
{
    uint32_t requestId;


    AccountId reportingAccountId;
    AccountId reportedAccountId;


    EntityId reportedSessionEntityId;


    uint64_t clientSimulationTick;
    uint64_t observedServerTick;


    std::string utf8Reason;


    ServerId serverId;
    MatchId matchId;
};
The client does not decide that the reported user is guilty.
The report creates an item for review.
The server/backend assigns:
* Report ID.
* UTC creation time.
* Server information.
* Match information.
* Relevant identity information.
* Evidence references.
________________


31. Report Evidence
A report should automatically attach useful context where legally and technically appropriate:
* Reported user ID.
* Reporting user ID.
* Server ID.
* Match ID.
* UTC timestamp.
* Authoritative server tick.
* Current map.
* Current game mode.
* Relevant server name.
* Recent public chat messages surrounding the report.
* Recent moderation actions involving the reported user.
* Whether the reported user was a guest.
* Client and server version.
* Protocol version.
Do not attach:
* Passwords.
* Authentication tokens.
* Private machine identifiers.
* Raw IP addresses to ordinary moderator views.
* Unrelated private data.
The moderation specification emphasizes that reports should actually work and that evidence should support a real investigation rather than disappearing into a black box. fileciteturn1file1L34-L40
________________


32. MiMITA Admin Moderation Queue
Route:
mimita.fun/admin
Add a moderation queue section.
Sort:
Most recent reports at the top
Each report card shows:
Report ID
Reported user
Reporting user
Reason
UTC report time
Authoritative server tick
Game/server
Match ID
Map
Game mode
Relevant recent messages
Guest or registered status
Current review status
Recommended statuses:
New
Reviewing
Needs more information
Action taken
No action
Duplicate
Escalated
Admin/moderator actions must be logged.
The queue should support:
* Search by username.
* Search by account ID.
* Search by report ID.
* Filter by status.
* Filter by server.
* Filter by date.
* Open the reported profile.
* Review attached public-chat evidence.
* Add internal notes.
* Record the final outcome.
Do not expose the moderation queue publicly.
Access is restricted to authorized MiMITA moderation/admin accounts.
________________


33. Replay Support
Accepted chat messages are recorded in replay data.
Replay events retain:
message ID
server tick
UTC timestamp
sender identity snapshot
sender stable ID
message
During replay playback:
* Chat appears at the corresponding replay tick.
* Typing indicators may optionally replay.
* Clicking usernames opens the profile preview using current information plus historical identity data.
* Deleted or renamed accounts remain identifiable through the historical snapshot and stable account ID.
* Report evidence may link directly to a replay timestamp when a replay exists.
________________


34. Popup Notification System
MiMITA has one reusable popup-notification system for the entire client.
Location:
Bottom-right corner
Maximum visible notifications:
3
Ordering:
Oldest at top
Newest at bottom
The notification system follows the GUI specification and must be JSON editable and hot reloadable. The GUI notes specifically require no hardcoded notification UI, a maximum of three visible popups, newest at the bottom, and reusable clickable notifications. fileciteturn2file0L131-L168
________________


35. Notification Structure
Conceptual structure:
enum class NotificationPriority : uint8_t
{
    Normal,
    Important,
    Critical
};


struct PopupNotification
{
    uint64_t notificationId;


    std::string title;
    std::string body;


    NotificationPriority priority;


    uint64_t createdUiTick;
    uint64_t durationUiTicks;


    std::optional<NotificationAction> action;
};
An action may:
* Open a GUI screen.
* Open a profile.
* Open a server.
* Open a replay.
* Open a safe MiMITA URL.
* Restore the tip box.
* Open moderation details for authorized users.
________________


36. Notification Behavior
When a fourth notification arrives while three are visible:
Recommended default:
1. Remove or archive the oldest non-critical notification.
2. Move the remaining notifications upward.
3. Add the newest notification at the bottom.
Critical notifications may replace lower-priority notifications first.
Each notification can be:
* Clicked.
* Dismissed with an X.
* Allowed to expire.
* Expanded when text is long.
Notification animation and duration use UI ticks rather than render frames.
________________


37. Notification Commands
Default:
notifs 1
Commands:
notifs 0
notifs 1
Temporary mute:
notifstempmute <hours>
Example:
notifstempmute 6
At 60 UI ticks per second:
6 hours
= 6 × 60 minutes
× 60 seconds
× 60 ticks
= 1,296,000 UI ticks
Use 64-bit tick storage.
The command accepts a human-readable hour value and converts it internally.
Recommended:
* Permit decimal hours if the console parser supports them.
* Store the final mute expiration as a monotonic deadline plus a persistent UTC deadline.
* Restore the mute correctly after restarting the game.
* Critical safety or account-security messages may bypass mute only if explicitly classified and documented.
________________


38. Tip-Box Hide Notification
When the player presses X on the floating tip box:
1. Hide the tip box.
2. Persist the preference.
3. Create a notification:
Tip box hidden


Use `tips 1` in the console to show it again.
The notification may include a clickable action:
UNDO
Clicking it immediately restores the tip box.
________________


39. JSON and Hot Reload Requirements
The following should be data-driven:
Chat window dimensions
Chat position
Chat opacity
Chat fonts
Chat colors
Role colors
Username effects
Hover-card appearance
Typing indicator appearance
World chat bubble appearance
Notification layout
Notification timing
Tip-box layout
Tip timing
Rate limits
Message history count
Maximum message length
Suggested files:
config/chatconfig.json
config/chatgui.json
config/tips.json
config/tipsconfig.json
config/notifications.json
config/profilepreview.json
config/rolestyles.json
Hot reload must:
* Parse into a temporary structure.
* Validate the entire structure.
* Swap it in only when valid.
* Keep the last valid configuration after an error.
* Log the exact file and field that failed.
* Not interrupt active networking.
* Not clear chat history unnecessarily.
* Not restart the server.
* Not create a large per-frame cost.
________________


40. Logging and Diagnostics
Development logs should include:
Chat request ID
Sender account/entity ID
Client tick
Server acceptance tick
Message byte length
Unicode code-point count
Accepted or rejected
Rejection reason
Rate-limit remaining ticks
Broadcast recipient count
Reliable-channel sequence
Typing-state transitions
Tip schedule tick
Tips-config reload result
Report submission result
Notification queue changes
Do not log sensitive authentication values.
Chat text logging should be configurable and handled consistently with moderation/privacy policy.
________________


41. Required Console Commands
chat <message>


chatwindow 0
chatwindow 1


chattips 0
chattips 1


tips 0
tips 1


listusers


chatmute <username>
chatmute <index>


reportuser


servermessage <message>


notifs 0
notifs 1


notifstempmute <hours>
Commands remain in the console.
Typing /command into chat does not execute commands.
Chat is for conversation.
The console is for commands.
________________


42. Implementation Completion Criteria
The feature is complete when all of the following work:
* / opens chat.
* Keyboard focus transfers correctly.
* Mouse unlock works.
* Enter sends.
* Escape cancels.
* Mouse lock restores correctly.
* Empty messages show the cute server response.
* Messages replicate between two clients.
* Messages replicate to and from the host.
* Solo play receives server messages and tips.
* Messages preserve order during packet loss and reordering.
* Maximum length is enforced safely for Unicode.
* Rate limiting is server authoritative.
* Chat history retains the latest 100 messages.
* Chat scroll works.
* New-message indicator works while scrolled upward.
* Username role colors work.
* Ultra-VIP per-letter styling works through reusable text rendering.
* Hovering a username shows a profile preview.
* Guests show the guest profile preview.
* Registered users show profile URL, join date, and MMR.
* Clicking profile links works safely.
* Typing state is visible in chat.
* Typing state is visible over player avatars.
* Typing state clears after disconnect or timeout.
* Tick numbers appear beside messages.
* UTC time appears on hover or according to configuration.
* chatwindow 0|1 works.
* chattips 0|1 works.
* tips 0|1 works.
* listusers works with confusing usernames.
* chatmute works with usernames and indexes.
* servermessage validates operator permissions.
* reportuser completes the interactive flow.
* Reports arrive at the mimita.fun/admin moderation queue.
* Report evidence contains tick, UTC time, server, match, and relevant chat context.
* tips.json hot reloads.
* tipsconfig.json hot reloads.
* Large tip intervals do not overflow.
* Tip scheduling has no meaningful main-thread cost.
* The floating tip box works across GUI screens.
* Pressing X hides the tip box and creates a notification.
* Notifications appear bottom-right.
* Maximum three notifications are visible.
* Newest notification appears at the bottom.
* Notifications are clickable.
* notifs 0|1 works.
* notifstempmute <hours> works.
* UI timing remains consistent at 30, 60, 144, 240, or irregular FPS.
* Chat events appear correctly in replays.
* No separate local, host, remote, or dedicated-server chat implementation exists.
________________


43. Recommended Implementation Order
Phase 1 — Core chat path
* Reuse existing working chat replication.
* Formalize ChatRequest and ChatMessageEvent.
* Add message IDs, server ticks, and UTC timestamps.
* Add 256-character UTF-8 validation.
* Add rate limiting.
* Add 100-message history.
Phase 2 — Input and interaction
* Implement / input capture.
* Implement Enter and Escape behavior.
* Implement mouse unlock and restoration.
* Implement scrolling.
* Implement username hover hitboxes.
Phase 3 — Profiles and typing
* Add registered-user preview.
* Add guest preview.
* Add profile-link action.
* Add networked typing state.
* Add world typing bubble.
Phase 4 — Tips
* Load tips.json.
* Create hot-reloadable tipsconfig.json.
* Implement efficient random scheduling.
* Add [server] Tip #N: formatting.
* Add the floating tip box to all major GUI screens.
Phase 5 — Reporting
* Implement listusers.
* Implement interactive reportuser.
* Submit stable user/server/match evidence.
* Add the moderation queue to mimita.fun/admin.
Phase 6 — Notifications
* Implement the reusable three-popup stack.
* Add GUI-tick timing.
* Add click actions and dismissal.
* Add notifs and notifstempmute.
* Connect the tip-box X button to a notification.
Phase 7 — Replay and tests
* Record chat events.
* Replay messages at their authoritative ticks.
* Test packet loss, duplication, and reordering.
* Test guests, registered users, host, operator, solo, and dedicated-server flows.
________________


44. Fundamental Rule
There is one chat path.
Client requests.
Server validates.
Server makes it real.
Server reliably broadcasts it.
Clients render it.
GUI systems, networking systems, profile systems, moderation systems, replay systems, and website systems share stable IDs and reusable data.
No duplicate local logic.
No frame-dependent timing.
No hardcoded one-off UI.
Everything important remains editable, observable, testable, and replaceable.