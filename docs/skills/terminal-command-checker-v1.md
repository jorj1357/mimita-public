// 09 03 2026, 15 41
/* purpose
* check that terminal commands have one clear owner and an honest contract
* ensure commands are discoverable, safe, testable, and consistent with behavior
* identify duplicate registration and commands that bypass shared actions
* this skill DOES NOT invent commands not required by the task or specification
* this skill DOES NOT allow unsafe production or secret-handling behavior
* this skill DOES NOT replace gameplay acceptance
*/

# Terminal Command Checker v1

Check registration, name, usage, description, ownership, input validation,
failure messages, and reuse of the shared action. Confirm help output and the
command's behavior match the specification. Flag duplicate names, hidden
side-effects, ambiguous arguments, and commands that cannot be tested without
the UI. Report exact paths, lines, contract differences, and focused checks.
