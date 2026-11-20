"""Stable command-line contract for the MiMITA development loop."""

from __future__ import annotations

import argparse


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="MiMITA incremental development loop")
    parser.add_argument("--profile", default="npc-navigation")
    parser.add_argument("--auto-restart", action="store_true")
    parser.add_argument("--launch-mode", help="numeric dev launch mode from devscripts/dev-launch-modes.json")
    visibility = parser.add_mutually_exclusive_group()
    visibility.add_argument("--headless", dest="launch_visibility", action="store_const",
                            const="headless", help="hide the game window while keeping the process and journal running")
    visibility.add_argument("--visible", dest="launch_visibility", action="store_const",
                            const="visible", help="show the game window without leaving it focused")
    parser.add_argument("--hot-client", action="store_true",
                        help="launch the full client with MIMITA_GAME_HOT_CLIENT=1")
    parser.add_argument("--status", action="store_true", help="read the current dev-loop status and exit")
    parser.add_argument("--list-instances", action="store_true",
                        help="print the current run's server/client instances and exit")
    parser.add_argument("--start-server", action="store_true",
                        help="start a server instance through the dev loop")
    parser.add_argument("--start-client", action="store_true",
                        help="start an additional client instance through the dev loop")
    parser.add_argument("--stop-instance", metavar="INSTANCE_ID",
                        help="stop one server/client instance")
    parser.add_argument("--restart-instance", metavar="INSTANCE_ID",
                        help="restart one server/client instance")
    parser.add_argument("--server", default="",
                        help="server instance id for --start-client")
    parser.add_argument("--inspect", metavar="PID_OR_INSTANCE_ID",
                        help="inspect one MiMITA process without changing it")
    parser.add_argument("--name", default="",
                        help="display name for --start-client")
    parser.add_argument("--badconn-preset", default="",
                        help="bad-connection preset for --start-client")
    parser.add_argument("--client", default="",
                        help="client instance id for --command (default: primary client)")
    parser.add_argument("--json", action="store_true", help="print control/status responses as JSON")
    parser.add_argument("--command", "--cmd", dest="command",
                        help="send one terminal command to the running daemon, wait for the result, and exit")
    parser.add_argument("--scenario-file", help="queue a repository-local Markdown/JSON scenario in the running client")
    parser.add_argument("--scenario-explain", action="store_true",
                        help="print the scenario purpose, steps, launch config, and assertions without running it")
    parser.add_argument("--ai-match", action="store_true",
                        help="run an unattended TDM match with the local player under AI control")
    parser.add_argument("--start-test-run", action="store_true",
                        help="start an isolated server/client run using the root dev-loop's current published build")
    parser.add_argument("--test-profile", default="tdm-validation",
                        help="profile used by --start-test-run (default: tdm-validation)")
    parser.add_argument("--test-launch-mode", default="41",
                        help="launch mode used by --start-test-run (default: 41 = TDM)")
    parser.add_argument("--test-map", default="",
                        help="optional map override for --start-test-run")
    parser.add_argument("--test-build-id", default="",
                        help="pin --start-test-run to a published build/artifact ID")
    parser.add_argument("--test-game-generation", type=int, default=0,
                        help="pin --start-test-run to a game DLL generation")
    parser.add_argument("--start-test-client", action="store_true",
                        help="add a second client to an online isolated test run for PvP")
    parser.add_argument("--test-client-name", default="",
                        help="display name for the second isolated test client")
    parser.add_argument("--list-test-runs", action="store_true",
                        help="list isolated test runs and their pinned build/journal paths")
    parser.add_argument("--run-id", default="",
                        help="isolated test run ID for --command or --stop-test-run")
    parser.add_argument("--stop-test-run", metavar="RUN_ID",
                        help="stop only the verified server/client processes owned by this isolated run")
    parser.add_argument("--ai-seconds", type=float, default=90.0, help="bounded duration for --ai-match")
    parser.add_argument("--ai-map", default="dust2cyberiav4", help="map for --ai-match")
    parser.add_argument("--ai-profile", default="tdm-validation", help="dev profile for --ai-match")
    parser.add_argument("--ai-launch-mode", default="41", help="dev launch mode for --ai-match (default 41 = TDM networking)")
    parser.add_argument("--ai-weapon", default="",
                        choices=("", "mace", "chainsaw", "swordsword"),
                        help="force one melee weapon and enable the melee validation path")
    parser.add_argument("--ai-health", type=int, default=999999,
                        help="self-only health override for the AI player (0 disables)")
    parser.add_argument("--runtime", default="", help="runtime id for runtime actions")
    parser.add_argument("--runtime-launch-mode", default="",
                        help="launch mode for --request start-runtime")
    parser.add_argument("--runtime-profile", default="npc-navigation")
    parser.add_argument("--runtime-map", default="")
    parser.add_argument("--no-runtime-client", action="store_true",
                        help="start the runtime server without a local client")
    parser.add_argument("--request", choices=("launch", "build", "game-dll", "agent-build", "cold-link-trial", "stop-client", "stop-server", "stop", "start-runtime", "stop-runtime", "reload-runtime", "list-runtimes"),
                        help="send a command to the already-running dev-loop daemon and exit; agents may request build/game-dll/cold-link-trial and are recorded by MIMITA_AGENT_ID")
    parser.add_argument("--build-request", default="",
                        help="approved coordination build request ID for --request agent-build")
    parser.add_argument("--wait-for-build", action="store_true",
                        help="wait for the requested build to finish; allowed for agent requests")
    parser.add_argument("--build-mode", choices=("debug", "release"), default="debug",
                        help="configuration for --request build")
    parser.add_argument("--build-timeout", type=float, default=36000.0,
                        help="maximum seconds to wait with --wait-for-build")
    parser.add_argument("--ensure-running", action="store_true", help="ensure the watchdog, daemon, and required dashboard exist and exit")
    parser.add_argument("--ensure-dashboard", action="store_true",
                        help="ensure exactly one visible coordinator dashboard exists")
    parser.add_argument("--dashboard-policy", choices=("on", "off", "status"),
                        default="",
                        help="persist whether the coordinator dashboard should be kept open")
    parser.add_argument("--no-dashboard", action="store_true",
                        help="ensure or control the dev loop without opening a dashboard")
    parser.add_argument("--daemon", action="store_true", help="run the dev-loop daemon in this process (used by the watchdog/entry point)")
    parser.add_argument("--dashboard", action="store_true", help="open the visible dashboard view attached to the daemon")
    parser.add_argument("--dev-loop-role", default="", choices=("", "watchdog", "daemon", "dashboard"),
                        help="stable role marker recorded in the process command line")
    parser.add_argument("--no-terminal-relaunch", action="store_true",
                        help="do not prefer Windows Terminal; render in the current console")
    parser.add_argument("--relaunch-terminal", action="store_true",
                        help="explicitly relaunch the dashboard inside Windows Terminal")
    parser.add_argument("--no-tk", action="store_true", help="use the terminal dashboard even when Tkinter is available")
    parser.add_argument("--dashboard-frames", type=int, default=0,
                        help="render this many dashboard frames then exit (0 = run until quit)")
    parser.add_argument("--command-timeout", type=float, default=15.0,
                        help="seconds to wait for a command result from the client")
    parser.add_argument("--coord-status", action="store_true",
                        help="show the shared agent coordination summary and exit")
    parser.add_argument("--coord-inbox", action="store_true",
                        help="show unread agent coordination messages and exit")
    parser.add_argument("--coord-message", metavar="MESSAGE_ID",
                        help="show one agent coordination message and exit")
    parser.add_argument("--coord-agent", default="",
                        help="agent name for coordination status (default: MIMITA_AGENT_ID)")
    parser.add_argument("--no-coord", action="store_true",
                        help="suppress the coordination summary in status output")
    return parser
