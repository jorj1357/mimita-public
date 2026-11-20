# Coordination decisions

This directory contains automatically published decisions from the shared agent
coordination brain.

- Live state and review pages: `.dev/agent-coord/`
- Source of truth: `.dev/agent-coord/brain.db`
- Event audit: `.dev/agent-coord/events.jsonl`
- Entry point: `python devscripts/agent-coord.py`
- Specification: `docs/specs/dev-loop/agent-coordination.md`

Each Markdown file records the task, conflict, input directives, chosen
approach, rejected alternatives, tradeoffs, affected paths, dissent, and
supersession history. Do not hand-edit a decision; record a new decision that
supersedes it.
