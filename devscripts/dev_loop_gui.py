"""Native Tkinter presentation for the MiMITA dev-loop dashboard."""

from __future__ import annotations

from typing import Callable


def available() -> bool:
    try:
        import tkinter  # noqa: F401
    except Exception:
        return False
    return True


def run(status_provider: Callable[[], dict],
        control: Callable[[str], bool],
        title: str) -> int:
    import tkinter as tk
    from tkinter import ttk

    root = tk.Tk()
    root.title(title)
    root.geometry("1120x680")
    root.minsize(860, 560)
    try:
        root.state("zoomed")
    except tk.TclError:
        pass
    root.configure(background="#101820")

    style = ttk.Style(root)
    try:
        style.theme_use("clam")
    except tk.TclError:
        pass
    style.configure("App.TFrame", background="#101820")
    style.configure("Card.TFrame", background="#1b2732", relief="flat")
    style.configure("Title.TLabel", background="#101820", foreground="#f4f7fa",
                    font=("Segoe UI", 18, "bold"))
    style.configure("Sub.TLabel", background="#101820", foreground="#9fb0be",
                    font=("Segoe UI", 9))
    style.configure("CardTitle.TLabel", background="#1b2732", foreground="#9fb0be",
                    font=("Segoe UI", 9, "bold"))
    style.configure("Value.TLabel", background="#1b2732", foreground="#f4f7fa",
                    font=("Segoe UI", 13, "bold"))
    style.configure("Detail.TLabel", background="#1b2732", foreground="#ced8df",
                    font=("Segoe UI", 9))
    style.configure("TButton", padding=(12, 8), font=("Segoe UI", 9, "bold"))

    outer = ttk.Frame(root, style="App.TFrame", padding=18)
    outer.pack(fill="both", expand=True)
    header = ttk.Frame(outer, style="App.TFrame")
    header.pack(fill="x", pady=(0, 14))
    ttk.Label(header, text="MiMITA Dev Loop", style="Title.TLabel").pack(anchor="w")
    subtitle = ttk.Label(header, text="Connecting to the checkout's build and runtime service…",
                         style="Sub.TLabel")
    subtitle.pack(anchor="w", pady=(4, 0))

    cards = ttk.Frame(outer, style="App.TFrame")
    cards.pack(fill="x")
    cards.columnconfigure(0, weight=1)
    cards.columnconfigure(1, weight=1)

    def make_card(parent, title_text: str, row: int, column: int):
        card = ttk.Frame(parent, style="Card.TFrame", padding=14)
        card.grid(row=row, column=column, sticky="nsew", padx=(0 if column == 0 else 7, 7 if column == 0 else 0),
                  pady=(0, 10))
        ttk.Label(card, text=title_text.upper(), style="CardTitle.TLabel").pack(anchor="w")
        value = ttk.Label(card, text="—", style="Value.TLabel", wraplength=480)
        value.pack(anchor="w", pady=(8, 5))
        detail = ttk.Label(card, text="", style="Detail.TLabel", wraplength=480,
                           justify="left")
        detail.pack(anchor="w")
        return value, detail

    build_value, build_detail = make_card(cards, "Selected build", 0, 0)
    phase_value, phase_detail = make_card(cards, "Build activity", 0, 1)
    runtime_value, runtime_detail = make_card(cards, "Runtime", 1, 0)
    scenario_value, scenario_detail = make_card(cards, "Scenario evidence", 1, 1)

    evidence = ttk.Frame(outer, style="Card.TFrame", padding=14)
    evidence.pack(fill="both", expand=True, pady=(0, 10))
    ttk.Label(evidence, text="BUILD OUTPUT AND RECENT ACTIVITY",
              style="CardTitle.TLabel").pack(anchor="w", pady=(0, 8))
    output = tk.Text(evidence, height=6, wrap="word", state="disabled",
                     background="#111a22", foreground="#d5e0e8",
                     insertbackground="#ffffff", relief="flat",
                     font=("Cascadia Mono", 9), padx=10, pady=9)
    output.pack(fill="both", expand=True)

    journal = ttk.Label(outer, text="Journal: (none)", style="Sub.TLabel", wraplength=1060)
    journal.pack(anchor="w", pady=(0, 10))
    action_status = tk.StringVar(value="Ready")
    ttk.Label(outer, textvariable=action_status, style="Sub.TLabel").pack(anchor="w")

    controls = ttk.Frame(outer, style="App.TFrame")
    controls.pack(fill="x", pady=(10, 0))
    state = {"frozen": False, "after_id": None}

    def refresh() -> None:
        if not state["frozen"]:
            try:
                model = status_provider() or {}
                build_value.configure(text=model.get("artifact_id") or "No successful build yet")
                build_detail.configure(text=(
                    f"{'STALE' if model.get('stale') else 'CURRENT'}  •  "
                    f"source generation {model.get('source_generation', '—')}\n"
                    f"Build ID {model.get('build_id', '—')}"
                ))
                phase_value.configure(text=f"{model.get('phase', 'unknown').upper()}  ·  "
                                            f"{model.get('elapsed', '—')}")
                phase_detail.configure(text=model.get("phase_detail") or "Waiting for build activity")
                runtime_value.configure(text=model.get("runtime", "Not running"))
                runtime_detail.configure(text=model.get("runtime_detail", ""))
                scenario = model.get("scenario") or {}
                scenario_value.configure(text=f"{scenario.get('verdict', '—')}  ·  "
                                                f"{scenario.get('id', '—')}")
                scenario_detail.configure(text=(
                    f"{scenario.get('state', 'No scenario recorded')}  ·  "
                    f"frame {scenario.get('frame', '—')}  ·  tick {scenario.get('tick', '—')}  ·  "
                    f"checkpoint {scenario.get('checkpoint', '—')} "
                    f"{scenario.get('checkpoint_progress', '')}  ·  "
                    f"capture {scenario.get('complete', 'unknown')}\n"
                    f"map {scenario.get('map', '—')}  ·  "
                    f"build {scenario.get('build_id', model.get('build_id', '—'))}\n"
                    f"{scenario.get('log_usage', '')}"
                ))
                journal.configure(text=f"Journal: {model.get('journal') or '(none)'}")
                subtitle.configure(text=(
                    f"{model.get('checkout', '')}    ·    daemon {model.get('daemon_pid', '—')}    ·    "
                    f"updated {model.get('updated_at', '—')}"
                ))
                lines = [*(model.get("build_output") or []), "", "Recent activity:",
                         *(model.get("recent") or ["(no recent activity)"])]
                output.configure(state="normal")
                output.delete("1.0", "end")
                output.insert("1.0", "\n".join(lines))
                output.configure(state="disabled")
            except Exception as error:
                action_status.set(f"Status refresh failed: {error}")
        if state["after_id"] is not None:
            try:
                root.after_cancel(state["after_id"])
            except tk.TclError:
                pass
        state["after_id"] = root.after(1000, refresh)

    def send(action: str) -> None:
        try:
            response = control(action)
            if isinstance(response, dict):
                accepted = bool(response.get("accepted"))
                message = response.get("message")
            else:
                accepted = bool(response)
                message = None
            action_status.set(message or f"{action}: {'accepted' if accepted else 'not accepted'}")
        except Exception as error:
            action_status.set(f"{action}: {error}")
        refresh()

    def toggle_freeze() -> None:
        state["frozen"] = not state["frozen"]
        pause_button.configure(text="Resume view" if state["frozen"] else "Pause view")
        action_status.set("View paused; daemon and game continue" if state["frozen"] else "View resumed")
        if not state["frozen"]:
            refresh()

    def close_view() -> None:
        root.destroy()

    for label, action in (
        ("Build / Launch", "build-or-launch"),
        ("Toggle visibility", "toggle-visibility"),
        ("Auto-restart", "toggle-auto-restart"),
        ("Stop daemon", "stop"),
    ):
        ttk.Button(controls, text=label,
                   command=lambda a=action: send(a)).pack(side="left", padx=(0, 7))
    pause_button = ttk.Button(controls, text="Pause view", command=toggle_freeze)
    pause_button.pack(side="left", padx=3)
    ttk.Button(controls, text="Close window", command=close_view).pack(side="right")

    root.bind("<Key-1>", lambda _e: send("build-or-launch"))
    root.bind("<Key-v>", lambda _e: send("toggle-visibility"))
    root.bind("<Key-a>", lambda _e: send("toggle-auto-restart"))
    root.bind("<Key-p>", lambda _e: toggle_freeze())
    root.bind("<Key-x>", lambda _e: close_view())
    refresh()
    root.mainloop()
    if state["after_id"] is not None:
        try:
            root.after_cancel(state["after_id"])
        except tk.TclError:
            pass
    return 0
