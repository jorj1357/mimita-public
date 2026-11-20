"""Focused tests for the shared agent coordination brain."""

from __future__ import annotations

import hashlib
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
        issue = self.store.issue_add(
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

    def test_integration_proof_requires_generation_and_events(self) -> None:
        self.begin("owner")
        issue = self.store.issue_add(
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
        issue = self.store.issue_add("owner", "Pure logic", source="agent")
        proof = self.store.evidence_record(
            "owner", issue["id"], proof_kind="unit")
        self.assertEqual(proof["result"], "proven")
        self.store.issue_close("owner", issue["id"], "unit verified")
        trace = self.store.issue_trace(issue["id"])
        self.assertEqual(trace["issue"]["state"], "done")
        self.assertEqual(trace["evidence"][0]["result"], "proven")

    def test_issue_clarification_visible_in_status_and_board(self) -> None:
        self.begin("owner")
        issue = self.store.issue_add(
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


if __name__ == "__main__":
    unittest.main()



