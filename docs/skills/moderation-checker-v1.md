// 09 03 2026, 15 41
/* purpose
* check that moderation actions follow the written safety and privacy behavior
* verify mute, unmute, block, reports, queues, and account visibility boundaries
* identify missing authorization, unclear ownership, and unsafe data exposure
* this skill DOES NOT invent moderation policy
* this skill DOES NOT print secrets or private account information
* this skill DOES NOT replace human review of harmful-content decisions
*/

# Moderation Checker v1

Trace each changed moderation action from user input through authorization,
storage, API or network transfer, and visible result. Check failure handling,
idempotency, auditability, and whether the correct user can see or change the
state. Compare every decision with the moderation specification and report
exact unsupported or missing behavior.
