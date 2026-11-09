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
    parser.add_argument("--ai-seconds", type=float, default=90.0, help="bounded duration for --ai-match")
    parser.add_argument("--ai-map", default="dust2cyberiav4", help="map for --ai-match")
    parser.add_argument("--ai-profile", default="tdm-validation", help="dev profile for --ai-match")
    parser.add_argument("--ai-launch-mode", default="41", help="dev launch mode for --ai-match (default 41 = TDM networking)")
    parser.add_argument("--request", choices=("launch", "build", "stop-client", "stop-server", "stop"),
                        help="send a command to the already-running dev-loop daemon and exit")
    parser.add_argument("--wait-for-build", action="store_true", help="with --request build, wait for this build request to finish")
    parser.add_argument("--build-mode", choices=("debug", "release"), default="debug",
                        help="configuration for --request build")
    parser.add_argument("--build-timeout", type=float, default=36000.0,
                        help="maximum seconds to wait with --wait-for-build")
    parser.add_argument("--ensure-running", action="store_true", help="ensure the watchdog and daemon exist and exit")
    parser.add_argument("--no-dashboard", action="store_true",
                        help="ensure or control the dev loop without opening a dashboard")
    parser.add_argument("--daemon", action="store_true", help="run the dev-loop daemon in this process (used by the watchdog/entry point)")
    parser.add_argument("--dashboard", action="store_true", help="open the visible dashboard view attached to the daemon")
    parser.add_argument("--dev-loop-role", default="", choices=("", "watchdog", "daemon", "dashboard"),
                        help="stable role marker recorded in the process command line")
    parser.add_argument("--no-terminal-relaunch", action="store_true",
                        help="do not prefer Windows Terminal; render in the current console")
    parser.add_argument("--no-tk", action="store_true", help="use the terminal dashboard even when Tkinter is available")
    parser.add_argument("--dashboard-frames", type=int, default=0,
                        help="render this many dashboard frames then exit (0 = run until quit)")
    parser.add_argument("--command-timeout", type=float, default=15.0,
                        help="seconds to wait for a command result from the client")
    return parser
