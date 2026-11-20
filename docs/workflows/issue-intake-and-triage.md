# Issue intake and triage

Use this workflow when a user reports a problem, an agent finds friction, or a
document contains a TODO that needs clarification.

## Rules

- Preserve the human report verbatim. Do not paraphrase it in place of the
  original.
- The shared brain is the live state. `docs/issues/` is the append-only durable
  record. Every new issue must include all required fields; incomplete creation
  is rejected. User messages are also recorded once by the automatic
  user-context relay and linked to the primary issue.
- Do not edit a spec or TODO to resolve an issue until the user has clarified
  the intended behavior.
- Every issue has a state, owner, and next action, or it is explicitly
  deferred with a reason.
- A task is complete only with its class-appropriate evidence.

## Steps

1. Register and synchronize:

   ```text
   python devscripts/agent-coord.py begin --agent <name> --task "<task>"
   python devscripts/agent-coord.py sync --agent <name>
   ```

2. Record the issue with the exact quote:

   ```text
   python devscripts/agent-issue.py --agent <name> --title "<title>" \
     --quote "<verbatim user report>" --source user --via <name> \
     --kind bug --severity high --summary "<scope>" \
     --reproduction "<steps>" --expected-behavior "<observable result>" \
     --acceptance "<measurable pass>" --paths <paths> \
     --evidence '{"source":"user"}' --risk "<risk>" \
     --proof-expectation "<runtime event/journal>"
   ```

   For a TODO scan, use `--source docs` and put the path, line, and exact text
   in the quote. Do not edit the document.

3. Triage it into tasks and steps:

   ```text
   python devscripts/agent-coord.py issue triage <issue-id> \
     --summary "<scope>" --owner <agent> --priority high
   python devscripts/agent-coord.py task add <step-1> --title "<step>" \
     --owner <agent> --issue <issue-id> --acceptance "<evidence>"
   python devscripts/agent-coord.py task add <step-2> ...
   python devscripts/agent-coord.py task dep <step-2> <step-1>
   python devscripts/agent-coord.py issue link-task <issue-id> <step-1>
   ```

4. Assign or hand off. Use `issue assign`, `task owner`, and
   `send --kind handoff.request`. A receiving agent accepts at its next
   activation with `reply` or `handoff.accepted`.

5. Defer explicitly:

   ```text
   python devscripts/agent-coord.py issue defer <issue-id> \
     --reason "<why>" --until "<UTC date/time>"
   ```

6. Ask the user when a document or report is ambiguous:

   ```text
   python devscripts/agent-coord.py issue clarify <issue-id> \
     --question "<exact question>"
   ```

   Keep the issue in `needs_clarification` until the user answers. Append the
   answer as a new quoted entry; never rewrite the original.

7. Record activity and evidence:

   ```text
   python devscripts/agent-coord.py activity set --phase working \
     --issue <issue-id> --paths <paths> --next-step "<next>"
   python devscripts/agent-coord.py issue review <issue-id> \
     --verdict pass --body "<evidence and limitations>"
   ```

8. Record proof before completion. A behavior task or issue needs a verified
   `runtime_proven` proof or a reasoned exemption:

   ```text
   python devscripts/agent-coord.py proof task-add <task> \
     --paths <paths> --build-id <id> --generation <n> \
     --run-id <id> --journal <events.jsonl> \
     --event <event> --field "<key>=<value>"
   python devscripts/agent-coord.py proof task-verify <proof-id>
   ```

   Docs/tooling-only work uses:

   ```text
   python devscripts/agent-coord.py proof exempt <task> \
     --paths <paths> --reason "<why runtime proof is not applicable>"
   ```

   Issue-level live evidence is recorded separately. `unit` is only for
   genuinely isolated logic; `integration` requires a pinned generation and
   events:

   ```text
   python devscripts/agent-coord.py proof record <issue-id> \
     --kind integration --generation <n> --run-id <run-id> \
     --server-journal <events.jsonl> --client-journal <events.jsonl> \
     --event <event>
   python devscripts/agent-coord.py proof verify <evidence-id>
   ```

   A documentation-only issue may use an explicit issue exemption:

   ```text
   python devscripts/agent-coord.py issue exempt <issue-id> \
     --reason "<why runtime proof is not applicable>" --paths <paths>
   ```

9. Close only with proof or exemption:

   ```text
   python devscripts/agent-coord.py issue close <issue-id> \
     --resolution "<what changed>" \
     --evidence "<changelog/build/run/journal/evidence-id>"
   ```

## Board

Run `python devscripts/agent-coord.py board` to see every agent's current
phase, task, issue, paths, build, game generation, run, blocker, and next step.
Use `--json` for tooling.

