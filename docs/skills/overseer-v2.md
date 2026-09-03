// 09 03 2026, 15 40
/* purpose
* tell an AI agent which focused skills to use for a repository task
* combine skill results into a clear pass, warning, or blocked result
* make skill usage visible in the final changelog
* this file DOES NOT replace the specifications
* this file DOES NOT decide whether desired behavior is correct
* this file DOES NOT make overseer.py the final authority
*/

# Skill Review Coordinator v2

Use the smallest set of skills that covers the task. Read the relevant
specification and architecture documents before using a skill.

## Default skill routes

- Documentation: `documentation-checker-v1.md`
- Logging: `logging-checker-v1.md`
- Performance: `efficiency-checker-v1.md`
- Terminal commands: `terminal-command-checker-v1.md`
- Chat: `chat-checker-v1.md`
- Moderation: `moderation-checker-v1.md`
- Assets: `asset-checker-v1.md`

## Required result

For every skill used, record its exact path, result, findings, unresolved
warnings, and evidence still required from human review. Do not run unrelated
skills merely to produce a green result.
