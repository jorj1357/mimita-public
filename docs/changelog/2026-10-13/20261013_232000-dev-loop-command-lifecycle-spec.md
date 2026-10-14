# Dev-loop command delivery and lifecycle specification

Date: 2026-10-13

Updated `docs/specs/dev-loop/dev-loop.md` to turn the user’s open questions
into an implementation contract.

Added:

- the five-stage command path from Python request to fixed-tick gameplay
  application;
- the diagnosis boundary for the failed `agent.move` test;
- required request, terminal, handler, and assertion events sharing one
  request ID;
- the target single-entry-point commands `dev-loop.py --command` and
  `--scenario-file` through the existing control socket;
- readiness, build-identity, queueing, and no-second-daemon requirements;
- daemon/client/server lifetime distinctions and recovery evidence for
  disappearing windows or closed scripts;
- phased implementation and a deterministic avatar walk/aim/shoot workflow.

No source code was changed in this update. The existing command adapter and
startup-command path remain explicitly marked as incomplete until fresh
runtime evidence reaches the handler-level events.

