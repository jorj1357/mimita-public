"""Focused tests for the shared agent coordination brain."""

from __future__ import annotations

import hashlib
import json
import os
import tempfile
import time
import unittest
from pathlib import Path

try:
    from devscripts.agent_coord import CoordError, CoordStore, coordination_brief
except ModuleNotFoundError:
    import sys
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    from devscripts.agent_coord import CoordError, CoordStore, coordination_brief

REPO = Path(__file__).resolve().parents[2]
TEMP_ROOT = REPO / ".dev" / "tmp"


def file_hash(relative: str) -> str:
    return hashlib.sha256((REPO / relative).read_bytes()).hexdigest()


class AgentCoordTests(unittest.TestCase):
    def setUp(self) -> None:
        TEMP_ROOT.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(
            prefix="agent-coord-", dir=TEMP_ROOT)
        self.root = Path(self.temporary.name) / "coord"
        self.store = CoordStore(self.root, publish_docs=False)
        self.previous_root = os.environ.get("MIMITA_COORD_ROOT")
        os.environ["MIMITA_COORD_ROOT"] = str(self.root)

    def tearDown(self) -> None:
        if self.previous_root is None:
            os.environ.pop("MIMITA_COORD_ROOT", None)
        else:
            os.environ["MIMITA_COORD_ROOT"] = self.previous_root
        self.store.close()
        self.temporary.cleanup()

    def begin(self, agent: str, task: str = "test") -> None:
        self.store.begin(agent, task)

    def add_issue(self, agent: str, title: str, **fields):
        fields.setdefault("quote", title)
        fields.setdefault("summary", title)
        fields.setdefault("reproduction", "test reproduction")
        fields.setdefault("expected_behavior", "test expected behavior")
        fields.setdefault("acceptance", "test acceptance")
        fields.setdefault("paths", ["AGENTS.md"])
        fields.setdefault("risk", "test risk")
        fields.setdefault("proof_expectation", "test proof")
        fields.setdefault("evidence", {"test": True})
        return self.store.issue_add(agent, title, **fields)

    def test_begin_and_sync_are_persistent(self) -> None:
        self.begin("agent-a", "coordination")
        self.store.task_add("agent-a", "A", "Task A")
        self.store.directive_add("agent-a", "use method X", task_id="A", method="X")
        self.store.note_add("agent-a", "found a dependency", task_id="A")
        data = self.store.sync("agent-a")
        self.assertTrue(data["events"])
        self.assertEqual(data["agent"], "agent-a")
        self.assertTrue(Path(self.root / "brain.db").is_file())
        self.assertTrue(Path(self.root / "events.jsonl").is_file())

    def test_method_conflict_and_autonomous_decision(self) -> None:
        self.begin("owner")
        self.store.task_add("owner", "B", "Task B")
        self.store.directive_add("owner", "method Y", task_id="B", method="Y")
        self.store.directive_add("owner", "method Z", task_id="B", method="Z")
        conflicts = self.store.conflict_list()
        self.assertEqual(len(conflicts), 1)
        decision = self.store.decision_record(
            "owner", "B", "use a combined method",
            conflict_id=conflicts[0]["conflict_id"],
            rationale="all recorded directives considered")
        self.assertTrue(decision["decision"])
        self.assertEqual(self.store.conflict_list(), [])
        self.assertEqual(self.store.decisions()[0]["status"], "active")
        self.store.directive_add("owner", "new constraint", task_id="B", method="W")
        self.assertEqual(self.store.decisions()[0]["status"], "stale")

    def test_vote_is_advisory(self) -> None:
        self.begin("owner")
        self.begin("reviewer")
        self.store.task_add("owner", "C", "Task C")
        self.store.directive_add("owner", "method A", task_id="C", method="A")
        self.store.directive_add("owner", "method B", task_id="C", method="B")
        conflict = self.store.conflict_list()[0]["conflict_id"]
        option = self.store.option_add("reviewer", conflict, "Option A",
                                       tradeoffs="known")["option"]
        self.store.vote("reviewer", conflict, option, "prefer A")
        decision = self.store.decision_record("owner", "C", "choose A",
                                              conflict_id=conflict)
        self.assertTrue(decision["decision"])
        votes = self.store.conn.execute(
            "SELECT * FROM votes WHERE conflict_id=?", (conflict,)).fetchall()
        self.assertEqual(len(votes), 1)

    def test_commit_releases_immediately(self) -> None:
        self.begin("agent-a")
        self.begin("agent-b")
        path = "AGENTS.md"
        self.store.edit_begin("agent-a", path)
        with self.assertRaises(CoordError) as raised:
            self.store.edit_begin("agent-b", path)
        self.assertEqual(raised.exception.code, 2)
        self.store.edit_commit("agent-a", path, new_hash=file_hash(path))
        acquired = self.store.edit_begin("agent-b", path)
        self.assertEqual(acquired["path"], path)
        self.store.edit_abort("agent-b", path)

    def test_failed_commit_still_releases(self) -> None:
        self.begin("agent-a")
        self.begin("agent-b")
        path = "AGENTS.md"
        self.store.edit_begin("agent-a", path)
        with self.assertRaises(CoordError) as raised:
            self.store.edit_commit("agent-a", path, new_hash="not-the-hash")
        self.assertEqual(raised.exception.code, 3)
        acquired = self.store.edit_begin("agent-b", path)
        self.assertEqual(acquired["path"], path)
        self.store.edit_abort("agent-b", path)
        self.assertTrue(self.store.conflict_list())

    def test_abort_releases(self) -> None:
        self.begin("agent-a")
        self.begin("agent-b")
        path = "AGENTS.md"
        self.store.edit_begin("agent-a", path)
        self.store.edit_abort("agent-a", path)
        self.assertEqual(self.store.edit_begin("agent-b", path)["path"], path)
        self.store.edit_abort("agent-b", path)

    def test_shared_regions_must_be_disjoint(self) -> None:
        self.begin("agent-a")
        self.begin("agent-b")
        with self.assertRaises(CoordError):
            self.store.shared_begin(
                "agent-a", "AGENTS.md", ["agent-b"],
                {"agent-a": ["same"], "agent-b": ["same"]})
        shared = self.store.shared_begin(
            "agent-a", "AGENTS.md", ["agent-b"],
            {"agent-a": ["left"], "agent-b": ["right"]})
        self.store.shared_accept("agent-b", shared["shared"])
        self.store.edit_begin("agent-a", "AGENTS.md", region="left")
        self.store.edit_commit("agent-a", "AGENTS.md")
        self.store.edit_begin("agent-b", "AGENTS.md", region="right")
        self.store.edit_abort("agent-b", "AGENTS.md")

    def test_shared_outsider_is_blocked(self) -> None:
        self.begin("agent-a")
        self.begin("agent-b")
        self.begin("agent-c")
        shared = self.store.shared_begin(
            "agent-a", "AGENTS.md", ["agent-b"],
            {"agent-a": ["left"], "agent-b": ["right"]})
        self.store.shared_accept("agent-b", shared["shared"])
        with self.assertRaises(CoordError):
            self.store.edit_begin("agent-c", "AGENTS.md")

    def test_halt_and_freeze_block_edits(self) -> None:
        self.begin("agent-a")
        self.store.halt("user", "pause agents")
        with self.assertRaises(CoordError):
            self.store.edit_begin("agent-a", "AGENTS.md")
        self.store.resume("user")
        self.store.freeze("user", "AGENTS.md", "do not touch")
        with self.assertRaises(CoordError):
            self.store.edit_begin("agent-a", "AGENTS.md")
        self.store.unfreeze("user", "AGENTS.md")
        self.store.edit_begin("agent-a", "AGENTS.md")
        self.store.edit_abort("agent-a", "AGENTS.md")

    def test_guard_blocks_another_agents_lease(self) -> None:
        self.begin("agent-a")
        self.begin("agent-b")
        self.store.edit_begin("agent-a", "AGENTS.md")
        blocked = self.store.guard("agent-b", paths=["AGENTS.md"])
        self.assertFalse(blocked["ok"])
        self.assertEqual(blocked["blocked"][0]["kind"], "lease")
        self.store.edit_commit("agent-a", "AGENTS.md")
        self.assertTrue(self.store.guard("agent-b", paths=["AGENTS.md"])["ok"])

    def test_messages_are_delivered_and_answered(self) -> None:
        self.begin("agent-a")
        self.begin("agent-b")
        sent = self.store.send("agent-a", "agent-b", "question",
                               "can you take this file?", kind="question",
                               requires_response=True, due_minutes=5)
        received = self.store.inbox("agent-b")
        self.assertEqual(received[0]["msg_id"], sent["message"])
        self.store.reply("agent-b", sent["message"], "yes, after this edit")
        answer = self.store.wait_for_answer("agent-a", sent["message"], 0.1)
        self.assertIsNotNone(answer)
        self.assertIn("yes", answer["body"])

    def test_broadcast_read_receipts_are_independent_per_agent(self) -> None:
        self.begin("agent-a")
        self.begin("agent-b")
        first = self.store.send(
            "agent-a", "all", "broadcast inbox", "each agent reads separately")
        received_a = self.store.inbox("agent-a")
        self.assertEqual(received_a[0]["msg_id"], first["message"])
        self.assertTrue(received_a[0]["read_at"])
        self.assertEqual(self.store.status("agent-b")["unread"], 1)
        received_b = self.store.inbox("agent-b")
        self.assertEqual(received_b[0]["msg_id"], first["message"])
        self.assertTrue(received_b[0]["read_at"])
        snapshot = self.store.coordination_snapshot()
        message = next(row for row in snapshot["messages"]
                       if row["msg_id"] == first["message"])
        self.assertEqual(
            {row["agent"] for row in message["read_receipts"]},
            {"agent-a", "agent-b"})

        second = self.store.send(
            "agent-a", "all", "broadcast sync", "sync receipts are per agent")
        sync_a = self.store.sync("agent-a")
        self.assertTrue(any(row["msg_id"] == second["message"]
                            for row in sync_a["messages"]))
        self.assertEqual(self.store.status("agent-b")["unread"], 1)
        sync_b = self.store.sync("agent-b")
        self.assertTrue(any(row["msg_id"] == second["message"]
                            for row in sync_b["messages"]))
        self.assertEqual(self.store.status("agent-a")["unread"], 0)
        self.assertEqual(self.store.status("agent-b")["unread"], 0)

    def test_path_escape_is_rejected(self) -> None:
        self.begin("agent-a")
        with self.assertRaises(CoordError):
            self.store.edit_begin("agent-a", "../outside.txt")

    def test_halt_guard_and_audit(self) -> None:
        self.begin("agent-a")
        self.store.halt("user", "stop")
        result = self.store.guard("agent-a", paths=["AGENTS.md"])
        self.assertFalse(result["ok"])
        self.assertEqual(result["blocked"][0]["kind"], "halt")
        self.store.resume("user")
        events = (self.root / "events.jsonl").read_text(encoding="utf-8")
        self.assertIn("coord.halt.set", events)
        self.assertIn('"category":"AGENT_COORD"', events)

    def test_brief_respects_environment(self) -> None:
        self.begin("agent-a")
        brief = coordination_brief("agent-a")
        self.assertEqual(brief["agents"][0]["agent"], "agent-a")

    def test_decision_document_is_published(self) -> None:
        self.store.publish_docs = True
        self.store.docs_root = self.root / "decisions"
        self.begin("owner")
        self.store.task_add("owner", "E", "Task E")
        self.store.directive_add("owner", "document it", task_id="E", method="X")
        decision = self.store.decision_record("owner", "E", "documented choice",
                                              rationale="because")
        self.assertTrue(decision["document"])
        self.assertTrue((REPO / decision["document"]).is_file())
        text = (REPO / decision["document"]).read_text(encoding="utf-8")
        self.assertIn("documented choice", text)
        self.assertIn("document it", text)

    def test_decision_stale_after_new_directive(self) -> None:
        self.begin("owner")
        self.store.task_add("owner", "D", "Task D")
        self.store.directive_add("owner", "one", task_id="D", method="one")
        decision = self.store.decision_record("owner", "D", "use one")
        self.assertEqual(self.store.decisions()[0]["status"], "active")
        time.sleep(0.01)
        self.store.directive_add("owner", "two", task_id="D", method="two")
        self.assertEqual(self.store.decisions()[0]["status"], "stale")
        self.assertTrue(decision["decision"])

    def test_issue_lifecycle_preserves_human_quote_and_appends(self) -> None:
        self.store.publish_docs = True
        self.store.docs_root = self.root / "decisions"
        self.store.issue_docs_root = self.root / "issues"
        self.begin("owner")
        self.begin("reviewer")
        quote = "The build took 18 minutes. Do not change this quote."
        issue = self.add_issue(
            "owner", "Build wait is too long", quote=quote,
            summary="slow build", kind="build-friction", source="user",
            via_agent="owner", severity="high")
        issue_id = issue["id"]
        self.assertTrue((self.root / "issues" / "README.md").is_file())
        self.store.issue_triage(
            "owner", issue_id, summary="measured cold build",
            owner="owner", priority="high")
        self.store.task_add(
            "owner", "T1", "Measure the build", owner="owner",
            issue_id=issue_id, priority="high",
            acceptance="timing JSONL and phase breakdown")
        self.store.issue_link_task("owner", issue_id, "T1")
        self.store.issue_defer("owner", issue_id, "waiting for baseline",
                               "2026-11-21T00:00:00Z")
        self.store.issue_quote("reviewer", issue_id,
                               "User said the build took 18 minutes.",
                               "supports the slow-build claim")
        self.store.issue_review("reviewer", issue_id, "needs-evidence",
                                "Need the same build measured twice.")
        with self.assertRaises(CoordError):
            self.store.issue_close("owner", issue_id, "safe seam identified",
                                   "timing JSONL")
        proof = self.store.evidence_record(
            "owner", issue_id, proof_kind="unit", required_events=[])
        self.assertEqual(proof["result"], "proven")
        self.store.issue_close("owner", issue_id, "safe seam identified",
                               proof["evidence"])
        shown = self.store.issue_show(issue_id)
        self.assertEqual(shown["state"], "done")
        self.assertEqual(shown["human_quote"], quote)
        self.assertEqual(shown["tasks"][0]["id"], "T1")
        self.assertEqual(shown["reviews"][0]["verdict"], "needs-evidence")
        self.assertEqual(shown["quotes"][0]["agent"], "reviewer")
        self.assertEqual(shown["quotes"][0]["supports"],
                         "supports the slow-build claim")
        doc = (REPO / shown["doc_path"]).read_text(encoding="utf-8")
        self.assertIn(quote, doc)
        self.assertIn("Triage", doc)
        self.assertIn("Review (needs-evidence)", doc)
        self.assertIn("safe seam identified", doc)

    def test_issue_proof_exemption_requires_reason_and_unblocks_close(self) -> None:
        self.begin("owner")
        issue = self.add_issue(
            "owner", "Documentation-only issue", quote="document this",
            kind="docs", source="user")
        with self.assertRaises(CoordError):
            self.store.issue_exempt("owner", issue["id"], "")
        self.store.issue_exempt(
            "owner", issue["id"],
            "documentation-only change; runtime event is not applicable",
            ["docs/issues/README.md"])
        self.store.issue_close("owner", issue["id"], "documented")
        self.assertEqual(self.store.issue_show(issue["id"])["state"], "done")

    def test_integration_proof_requires_generation_and_events(self) -> None:
        self.begin("owner")
        issue = self.add_issue(
            "owner", "Needs integration proof", source="user",
            quote="prove it in the live exe")
        proof = self.store.evidence_record(
            "owner", issue["id"], proof_kind="integration",
            build_generation=0, required_events=["scenario.finished"])
        self.assertEqual(proof["result"], "unproven")
        with self.assertRaises(CoordError):
            self.store.issue_close("owner", issue["id"], "done")

    def test_unit_proof_closes_and_trace_contains_evidence(self) -> None:
        self.begin("owner")
        issue = self.add_issue("owner", "Pure logic", source="agent")
        proof = self.store.evidence_record(
            "owner", issue["id"], proof_kind="unit")
        self.assertEqual(proof["result"], "proven")
        self.store.issue_close("owner", issue["id"], "unit verified")
        trace = self.store.issue_trace(issue["id"])
        self.assertEqual(trace["issue"]["state"], "done")
        self.assertEqual(trace["evidence"][0]["result"], "proven")

    def test_next_work_claims_high_priority_issue(self) -> None:
        self.begin("owner")
        low = self.add_issue("owner", "low work", priority="low")
        high = self.add_issue("owner", "high work", priority="high")
        result = self.store.next_work("owner", claim=True)
        self.assertEqual(result["issue"]["id"], high["id"])
        self.assertEqual(result["issue"]["owner"], "owner")
        self.assertEqual(result["next_step"], "high work")

    def test_observe_bundle_contains_issue_and_evidence(self) -> None:
        self.begin("owner")
        issue = self.add_issue("owner", "observe me", source="user",
                                     quote="watch this")
        self.store.evidence_record("owner", issue["id"], proof_kind="unit")
        bundle = self.store.observe_issue(issue["id"], "owner")
        self.assertEqual(bundle["issue"]["id"], issue["id"])
        self.assertEqual(bundle["evidence"][0]["result"], "proven")
        self.assertTrue(bundle["build"])

    def test_build_queue_and_runtime_registry(self) -> None:
        self.begin("owner", "sub",)
        self.begin("coordinator")
        self.store.build_owner("coordinator")
        request = self.store.build_request(
            "owner", kind="preview", issue_id="I1", base_generation=7,
            paths=["src/a.cpp"], overlay_paths=["src/a.cpp"],
            priority="high", cohort="main")
        self.assertEqual(request["status"], "proposed")
        self.store.build_update("coordinator", request["request"], "approved")
        self.store.build_wait("owner", request["request"], "slot busy", 42)
        rows = self.store.build_list()
        self.assertEqual(rows[0]["status"], "approved")
        self.assertIn("slot busy", rows[0]["wait_reason"])
        runtime = self.store.runtime_register(
            "owner", "runtime-1", cohort="main", issue_id="I1",
            generation=7, dll_path="build/game/generation-7/game-g7.dll",
            dll_sha256="abc", run_id="run-1", server_pid=10,
            client_pids=[11], server_journal="server/events.jsonl",
            client_journals=["client/events.jsonl"], state="running")
        self.assertEqual(runtime["state"], "running")
        self.assertEqual(self.store.runtime_list(agent="owner")[0]["generation"], 7)
        self.store.runtime_update("owner", "runtime-1", "stopped")
        self.assertEqual(self.store.runtime_list(agent="owner")[0]["state"], "stopped")

    def test_subagent_parent_and_cohort_columns(self) -> None:
        self.store.begin("parent", "parent task", cohort="map")
        self.store.begin("child", "child task", parent="parent", cohort="map")
        row = self.store.conn.execute(
            "SELECT parent,cohort FROM agents WHERE agent='child'").fetchone()
        self.assertEqual(row["parent"], "parent")
        self.assertEqual(row["cohort"], "map")

    def test_wake_check_returns_actionable_and_idle_states(self) -> None:
        self.begin("owner")
        idle = self.store.wake_check("owner")
        self.assertFalse(idle["wake"])
        self.assertEqual(idle["exit_code"], 10)
        issue = self.add_issue("owner", "wake me", priority="high")
        active = self.store.wake_check("owner")
        self.assertTrue(active["wake"])
        self.assertEqual(active["exit_code"], 0)
        self.assertEqual(active["issue"]["id"], issue["id"])
        self.assertEqual(active["next_step"], "wake me")

    def test_issue_clarification_visible_in_status_and_board(self) -> None:
        self.begin("owner")
        issue = self.add_issue(
            "owner", "TODO needs clarification", quote="unclear requirement",
            kind="todo", source="docs")
        self.store.issue_clarify("owner", issue["id"],
                                 "Should this TODO be implemented or removed?")
        self.assertEqual(issue["state"], "new")
        status = self.store.status("owner")
        self.assertTrue(any(row["id"] == issue["id"] for row in status["issues"]))
        board = self.store.board(agent="owner")
        row = next(item for item in board["issues"] if item["id"] == issue["id"])
        self.assertEqual(row["state"], "needs_clarification")
        self.assertIn("implemented or removed", row["clarification"])

    def test_proof_gate_blocks_until_verified_runtime_evidence(self) -> None:
        self.begin("owner")
        source = self.root / "proof-source.txt"
        source.write_text("one\n", encoding="utf-8")
        relative = str(source.relative_to(REPO))
        journal = self.root / "events.jsonl"
        journal.write_text(json.dumps({
            "event": "runtime.proven", "fields": {"npcs": 67},
        }) + "\n", encoding="utf-8")
        self.store.task_add("owner", "P1", "Runtime proof task", owner="owner",
                            acceptance="event observed")
        with self.assertRaises(CoordError) as raised:
            self.store.task_status("owner", "P1", "done")
        self.assertEqual(raised.exception.code, 5)
        proof = self.store.proof_add(
            "owner", "P1", paths=[relative], status="runtime_proven",
            build_id="build-1", game_generation=7, run_id="run-1",
            journal_path=str(journal.relative_to(REPO)),
            event="runtime.proven", field_predicate="npcs=67")
        self.store.proof_verify("owner", proof["id"])
        self.store.task_status("owner", "P1", "done")
        source.write_text("two\n", encoding="utf-8")
        status = self.store.proof_status()
        self.assertGreaterEqual(status["stale"], 1)

    def test_proof_exempt_requires_reason_and_unblocks(self) -> None:
        self.begin("owner")
        self.store.task_add("owner", "P2", "Docs task", owner="owner")
        with self.assertRaises(CoordError):
            self.store.proof_exempt("owner", "P2", "")
        self.store.proof_exempt("owner", "P2",
                                "documentation-only change with diff check",
                                ["AGENTS.md"])
        self.assertGreaterEqual(self.store.proof_status()["exempt"], 1)
        self.store.task_status("owner", "P2", "done")

    def test_activity_board_and_deferred_task(self) -> None:
        self.begin("owner")
        self.store.task_add("owner", "T2", "Waited for build", owner="owner")
        self.store.activity_set(
            "owner", task_id="T2", phase="waiting", activity="waiting for build",
            paths=["src/gamemode/gamemode.cpp"], build_id="build-1",
            game_generation=7, run_id="run-1", blocker="build queue",
            next_step="start pinned run", wait_reason="cold EXE build")
        status = self.store.status("owner")
        activity = next(row for row in status["activity"]
                        if row["agent"] == "owner")
        self.assertEqual(activity["wait_reason"], "cold EXE build")
        board = self.store.board(agent="owner")
        row = next(item for item in board["agents"] if item["agent"] == "owner")
        self.assertEqual(row["activity"]["build_id"], "build-1")
        self.assertEqual(row["activity"]["game_generation"], 7)

    def test_coordination_snapshot_is_bounded_and_does_not_mark_read(self) -> None:
        self.begin("owner")
        self.store.activity_set(
            "owner", phase="working", activity="reading the board",
            paths=["src/gamemode/gamemode.cpp"], next_step="run pinned test")
        issue = self.add_issue(
            "owner", "Panel issue", kind="improvement", source="user")
        session = self.store.work_start("owner", issue_id=issue["id"])
        sent = self.store.send_human(
            "owner", "Dashboard report", "Please check the panel.")
        self.assertEqual(sent["sender"], "human-dashboard")
        snapshot = self.store.coordination_snapshot(limit_messages=10)
        self.assertEqual(snapshot["agents"][0]["agent"], "owner")
        self.assertEqual(snapshot["issues"][0]["id"], issue["id"])
        message = next(row for row in snapshot["messages"]
                       if row["msg_id"] == sent["message"])
        self.assertEqual(message["sender_kind"], "human")
        self.assertEqual(snapshot["work"][0]["session_id"],
                         session["session_id"])
        self.assertGreater(snapshot["unread"], 0)
        # A dashboard read must not consume the message.
        self.assertEqual(snapshot["unread"],
                         self.store.coordination_snapshot()["unread"])
        inbox = self.store.inbox("owner", unread_only=True)
        self.assertTrue(any(row["msg_id"] == sent["message"] for row in inbox))

    def test_issue_brief_includes_tasks_and_next_action(self) -> None:
        self.begin("owner")
        issue = self.add_issue(
            "owner", "Brief me", source="user", summary="handoff")
        self.store.task_add(
            "owner", "T1", "Do the work", owner="owner",
            issue_id=issue["id"], acceptance="journal proof")
        brief = self.store.issue_brief(issue["id"])
        self.assertEqual(brief["issue"]["id"], issue["id"])
        self.assertEqual(brief["tasks"][0]["id"], "T1")
        self.assertEqual(brief["next"]["action"], "claim_task")
        self.assertEqual(brief["missing"], [])

    def test_issue_occurrence_records_recurrence(self) -> None:
        self.store.ensure_agent("launch-doctor", "diagnosis")
        self.store.ensure_agent("launch-doctor", "diagnosis")
        issue = self.add_issue(
            "launch-doctor", "Launch failure: timeout at server",
            kind="launch-friction", source="dev-loop")
        self.store.issue_occurrence(
            "launch-doctor", issue["id"], "Failure recurred",
            {"launch_id": "L2"})
        shown = self.store.issue_show(issue["id"])
        self.assertEqual(len(shown["occurrences"]), 1)
        self.assertEqual(shown["occurrences"][0]["body"], "Failure recurred")

    def test_send_human_rejects_unknown_recipient(self) -> None:
        with self.assertRaises(CoordError):
            self.store.send_human("missing-agent", "hello", "body")

    def test_issue_context_readiness(self) -> None:
        self.begin("context-agent")
        issue = self.add_issue(
            "context-agent", "Screenshot is missing", quote="take a screenshot",
            kind="bug", source="user", evidence={"text": "no PNG"})
        with self.store.conn:
            self.store.conn.execute(
                "UPDATE issues SET reproduction='' WHERE id=?", (issue["id"],))
        initial = self.store.issue_readiness(issue["id"])
        self.assertFalse(initial["ready"])
        self.assertIn("reproduction", initial["missing"])
        self.store.issue_context(
            "context-agent", issue["id"],
            reproduction="run screenshot_at_tick 120",
            expected_behavior="PNG and sidecar exist",
            acceptance="sidecar tick equals 120",
            paths=["src/game/screenshot.cpp"])
        ready = self.store.issue_readiness(issue["id"])
        self.assertTrue(ready["ready"], ready)
        self.assertEqual(ready["missing"], [])
        self.assertEqual(ready["score"], ready["possible"])
        self.assertEqual(self.store.issue_show(issue["id"])["acceptance"],
                         "sidecar tick equals 120")

    def test_user_context_is_deduplicated_broadcast_and_acknowledged(self) -> None:
        self.begin("agent-a")
        self.begin("agent-b")
        first = self.store.user_context_add(
            "agent-a", "Keep this exact sentence.", session="s1",
            source_ref="s1:1", priority="high")
        second = self.store.user_context_add(
            "agent-a", "Keep this exact sentence.", session="s1",
            source_ref="s1:1", priority="high")
        self.assertEqual(first["context_id"], second["context_id"])
        self.assertFalse(second["created"])
        sync = self.store.sync("agent-b")
        self.assertEqual(sync["user_context"][0]["quote"],
                         "Keep this exact sentence.")
        self.assertTrue(any(message["kind"] == "user-context"
                            for message in sync["messages"]))
        self.store.user_context_ack("agent-b", first["context_id"])
        self.assertEqual(self.store.user_context_list(
            "agent-b", unread_only=True), [])

    def test_user_context_link_move_and_lint(self) -> None:
        self.begin("owner")
        context = self.store.user_context_add(
            "owner", "This belongs to another issue.", priority="high")
        first = self.add_issue("owner", "First owner")
        second = self.add_issue("owner", "Better owner")
        self.store.user_context_link(
            "owner", context["context_id"], first["id"], reason="initial")
        moved = self.store.user_context_move(
            "owner", context["context_id"], second["id"], "routing correction")
        self.assertEqual(moved["canonical_issue_id"], second["id"])
        self.assertTrue(any(link["issue_id"] == second["id"] and link["active"]
                            for link in moved["links"]))
        self.assertIn("stale_canonical", self.store.user_context_lint())

    def test_issue_add_rejects_missing_required_fields(self) -> None:
        self.begin("owner")
        with self.assertRaises(CoordError) as raised:
            self.store.issue_add("owner", "incomplete", quote="q")
        self.assertIn("summary", str(raised.exception))
        self.assertIn("paths", str(raised.exception))

    def test_begin_opens_and_end_closes_work_session(self) -> None:
        self.begin("auto-agent")
        self.assertEqual(self.store.work_summary(agent="auto-agent")["active"], 1)
        self.store.end("auto-agent")
        summary = self.store.work_summary(agent="auto-agent")
        self.assertEqual(summary["active"], 0)
        self.assertEqual(summary["sessions"], 1)

    def test_build_wait_is_distinct_and_auto_resumes(self) -> None:
        self.begin("wait-agent")
        session = self.store.work_start("wait-agent", task_id="build")
        self.store.work_phase("wait-agent", session["session_id"], "editing")
        waiting = self.store.work_wait(
            "wait-agent", "build_wait", wait_reason="game DLL build",
            build_id="request-1")
        self.assertEqual(waiting["phase"], "build_wait")
        self.assertEqual(waiting["resume_phase"], "editing")
        resumed = self.store.work_resume("wait-agent")
        self.assertEqual(resumed["phase"], "editing")
        self.assertEqual(resumed["resume_phase"], "")
        summary = self.store.work_summary(agent="wait-agent")
        self.assertEqual(summary["sessions"], 1)
        self.assertIn("build_wait", summary["phases_ms"])

    def test_work_sessions_measure_received_to_done(self) -> None:
        self.begin("work-agent")
        started = self.store.work_start(
            "work-agent", task_id="test", issue_id="issue-1",
            message_id="msg-1", notes="received request")
        session = started["session_id"]
        time.sleep(0.02)
        self.store.work_phase("work-agent", session, "planning")
        self.store.work_phase("work-agent", session, "editing")
        active = self.store.work_list(agent="work-agent", active_only=True)
        self.assertEqual(active[0]["session_id"], session)
        finished = self.store.work_end("work-agent", session)
        self.assertEqual(finished["state"], "done")
        self.assertGreaterEqual(finished["elapsed_ms"], 20)
        names = [row["name"] for row in finished["phases"]]
        self.assertEqual(names, ["received", "planning", "editing"])
        self.assertTrue(all(row["state"] == "done"
                            for row in finished["phases"]))
        summary = self.store.work_summary(agent="work-agent")
        self.assertEqual(summary["sessions"], 1)
        self.assertEqual(summary["active"], 0)
        self.assertIn("planning", summary["phases_ms"])
        self.assertGreaterEqual(summary["phases_ms"]["planning"], 0)


if __name__ == "__main__":
    unittest.main()



