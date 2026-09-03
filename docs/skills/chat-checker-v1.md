// 09 03 2026, 15 41
/* purpose
* check that chat behavior, ownership, display, and networking agree
* preserve the intended distinction between chat, HUD, and killfeed output
* identify missing events, duplicate messages, and incorrect visibility
* this skill DOES NOT change moderation policy without its specification
* this skill DOES NOT claim visual acceptance from source inspection alone
* this skill DOES NOT expose private messages or account data
*/

# Chat Checker v1

Read the chat specification, then trace input or event to packet, history,
filter, and rendered output. Check local and remote behavior, ordering,
duplicate suppression, mode selection, and configuration ownership. Report the
first missing or wrong step with exact paths and lines. Separate source proof
from the human test still needed to confirm what appears on screen.
