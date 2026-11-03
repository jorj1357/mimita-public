"""Native, compact Tk dashboard for the MiMITA dev-loop daemon."""

from __future__ import annotations

import colorsys
import ctypes
import os
import time
from pathlib import Path
from typing import Callable


BLACK = "#000000"
RED = "#ff0000"
GREEN = "#00ff00"
WHITE = "#ffffff"
FONT_PATH = Path(r"C:\important\mingliu-font\mingliu.ttf")
MIN_WIDTH = 400
MIN_HEIGHT = 500
APP_USER_MODEL_ID = "MiMITA.DevLoop.Dashboard"


def available() -> bool:
    try:
        import tkinter  # noqa: F401
    except Exception:
        return False
    return True


def _private_font():
    """Load MingLiU for this window process without installing it globally."""
    if os.name != "nt" or not FONT_PATH.is_file():
        return None
    try:
        gdi = ctypes.WinDLL("gdi32", use_last_error=True)
        gdi.AddFontResourceExW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint, ctypes.c_void_p]
        gdi.AddFontResourceExW.restype = ctypes.c_int
        loaded = gdi.AddFontResourceExW(str(FONT_PATH), 0x10, None)
        return gdi if loaded else None
    except (AttributeError, OSError):
        return None


def _artifact_key(value: str) -> str:
    value = value.replace("LEGACY #", "").strip()
    if value.isdecimal():
        return str(int(value))
    return value
def run(status_provider: Callable[[], dict],
        control: Callable[[str], dict | bool],
        title: str) -> int:
    import tkinter as tk

    if os.name == "nt":
        try:
            shell = ctypes.WinDLL("shell32", use_last_error=True)
            set_app_id = shell.SetCurrentProcessExplicitAppUserModelID
            set_app_id.argtypes = [ctypes.c_wchar_p]
            set_app_id.restype = ctypes.c_long
            result = set_app_id(APP_USER_MODEL_ID)
            if result < 0:
                raise OSError(result, "could not register dashboard app identity")
        except (AttributeError, OSError) as error:
            # The top-level WS_EX_APPWINDOW style still keeps the window in the
            # taskbar and Alt+Tab if the optional shell grouping ID is unavailable.
            app_id_error = str(error)
        else:
            app_id_error = ""
    else:
        app_id_error = ""

    font_library = _private_font()
    family = "MingLiU" if font_library else "Courier New"
    root = tk.Tk()
    root.title(title)
    root.overrideredirect(True)
    root.resizable(True, True)
    root.minsize(MIN_WIDTH, MIN_HEIGHT)
    root.geometry(f"{MIN_WIDTH}x{MIN_HEIGHT}")
    root.configure(background=BLACK)

    state = {
        "frozen": False,
        "after_id": None,
        "hue_started": time.monotonic(),
        "maximized": False,
        "restore_geometry": None,
        "drag": None,
        "resize": None,
        "compact": None,
        "wide": None,
        "identity_match": False,
        "app_id_error": app_id_error,
        "phase_color_mode": "rainbow",
        "animation_id": None,
        "minimized": False,
        "pending_request_id": "",
    }

    def frame(parent, *, padx=0, pady=0, **kwargs):
        return tk.Frame(parent, bg=BLACK, highlightthickness=1,
                        highlightbackground=RED, highlightcolor=RED,
                        padx=padx, pady=pady, **kwargs)

    def label(parent, text="", *, size=9, bold=False, color=RED, **kwargs):
        weight = "bold" if bold else "normal"
        return tk.Label(parent, text=text, bg=BLACK, fg=color,
                        font=(family, size, weight), anchor="w",
                        justify="left", **kwargs)

    def button(parent, text, callback, *, width=None):
        options = {
            "text": text,
            "command": callback,
            "bg": BLACK,
            "fg": RED,
            "activebackground": "#330000",
            "activeforeground": WHITE,
            "disabledforeground": "#660000",
            "relief": "raised",
            "bd": 2,
            "highlightthickness": 1,
            "highlightbackground": RED,
            "highlightcolor": RED,
            "font": (family, 8, "bold"),
            "padx": 5,
            "pady": 2,
            "takefocus": True,
        }
        if width is not None:
            options["width"] = width
        return tk.Button(parent, **options)

    # Make a taskbar entry for the borderless custom-caption window.
    if os.name == "nt":
        def mark_as_app_window():
            try:
                hwnd = root.winfo_id()
                user32 = ctypes.WinDLL("user32", use_last_error=True)
                from ctypes import wintypes

                user32.GetAncestor.argtypes = [wintypes.HWND, wintypes.UINT]
                user32.GetAncestor.restype = wintypes.HWND
                get_long = getattr(user32, "GetWindowLongPtrW", None) or user32.GetWindowLongW
                set_long = getattr(user32, "SetWindowLongPtrW", None) or user32.SetWindowLongW
                get_long.argtypes = [wintypes.HWND, ctypes.c_int]
                get_long.restype = ctypes.c_ssize_t
                set_long.argtypes = [wintypes.HWND, ctypes.c_int, ctypes.c_ssize_t]
                set_long.restype = ctypes.c_ssize_t
                user32.SetWindowPos.argtypes = [wintypes.HWND, wintypes.HWND,
                                                ctypes.c_int, ctypes.c_int,
                                                ctypes.c_int, ctypes.c_int, wintypes.UINT]
                user32.ShowWindow.argtypes = [wintypes.HWND, ctypes.c_int]
                user32.ShowWindow.restype = wintypes.BOOL
                # winfo_id may identify Tk's client child. Shell visibility
                # belongs to the real top-level window returned by GA_ROOT.
                hwnd = user32.GetAncestor(hwnd, 2) or hwnd
                ex_style = get_long(hwnd, -20)
                app_style = (ex_style | 0x00040000) & ~0x00000080
                set_long(hwnd, -20, app_style)
                user32.SetWindowPos(hwnd, None, 0, 0, 0, 0,
                                    0x0001 | 0x0002 | 0x0004 | 0x0020)
                # Force Explorer and the task switcher to refresh the frame's
                # app-window registration after Tk's override-redirect change.
                user32.ShowWindow(hwnd, 5)
                if get_long(hwnd, -20) != app_style:
                    state["window_registration_error"] = (
                        "Windows did not retain the taskbar/app-switcher window style")
            except (AttributeError, OSError):
                state["window_registration_error"] = "Windows app-window registration failed"

        root.after(180, mark_as_app_window)

    shell = tk.Frame(root, bg=BLACK, highlightthickness=1,
                     highlightbackground=RED, highlightcolor=RED)
    shell.pack(fill="both", expand=True)
    shell.grid_rowconfigure(1, weight=1)
    shell.grid_columnconfigure(0, weight=1)

    titlebar = tk.Frame(shell, bg=BLACK, height=27,
                        highlightthickness=0, bd=0)
    titlebar.grid(row=0, column=0, sticky="ew")
    titlebar.grid_columnconfigure(0, weight=1)
    app_title = label(titlebar, title, size=9, bold=True)
    app_title.grid(row=0, column=0, sticky="w", padx=(8, 4), pady=3)

    content = tk.Frame(shell, bg=BLACK, padx=7, pady=5)
    content.grid(row=1, column=0, sticky="nsew")
    content.grid_columnconfigure(0, weight=1)
    content.grid_rowconfigure(3, weight=1)

    caption_buttons = tk.Frame(titlebar, bg=BLACK)
    caption_buttons.grid(row=0, column=1, sticky="e", padx=(0, 4))

    def minimize():
        if os.name == "nt":
            try:
                user32 = ctypes.WinDLL("user32")
                user32.ShowWindow.argtypes = [ctypes.c_void_p, ctypes.c_int]
                user32.ShowWindow(root.winfo_id(), 6)
                state["minimized"] = True
                return
            except (AttributeError, OSError):
                pass
        root.iconify()

    def toggle_maximize():
        if state["maximized"]:
            geometry = state["restore_geometry"] or f"{MIN_WIDTH}x{MIN_HEIGHT}"
            root.geometry(geometry)
            state["maximized"] = False
            maximize_button.configure(text="□")
            return
        state["restore_geometry"] = root.geometry()
        if os.name == "nt":
            try:
                from ctypes import wintypes

                rect = wintypes.RECT()
                user32 = ctypes.WinDLL("user32")
                user32.SystemParametersInfoW.argtypes = [wintypes.UINT, wintypes.UINT,
                                                         ctypes.c_void_p, wintypes.UINT]
                user32.SystemParametersInfoW(
                    0x0030, 0, ctypes.byref(rect), 0)
                width, height = rect.right - rect.left, rect.bottom - rect.top
                root.geometry(f"{width}x{height}+{rect.left}+{rect.top}")
            except (AttributeError, OSError):
                root.geometry(f"{root.winfo_screenwidth()}x{root.winfo_screenheight()}+0+0")
        else:
            root.geometry(f"{root.winfo_screenwidth()}x{root.winfo_screenheight()}+0+0")
        state["maximized"] = True
        maximize_button.configure(text="❐")

    def close_view():
        root.destroy()

    button(caption_buttons, "_", minimize, width=2).pack(side="left", padx=1)
    maximize_button = button(caption_buttons, "□", toggle_maximize, width=2)
    maximize_button.pack(side="left", padx=1)
    button(caption_buttons, "X", close_view, width=2).pack(side="left", padx=1)

    def start_move(event):
        if state["maximized"]:
            return
        state["drag"] = (event.x_root - root.winfo_x(),
                         event.y_root - root.winfo_y())

    def move_window(event):
        drag = state["drag"]
        if drag:
            root.geometry(f"+{event.x_root - drag[0]}+{event.y_root - drag[1]}")

    for widget in (titlebar, app_title):
        widget.bind("<ButtonPress-1>", start_move)
        widget.bind("<B1-Motion>", move_window)
        widget.bind("<ButtonRelease-1>", lambda _event: state.__setitem__("drag", None))

    # Custom resize edges retain the normal window's resize behavior after the
    # native title bar has been replaced with the black caption above.
    resize_edges = (
        ("n", "size_ns", {"x": 7, "y": 0, "relwidth": 1, "width": -14, "height": 6}),
        ("s", "size_ns", {"x": 7, "rely": 1, "relwidth": 1, "width": -14, "height": 6}),
        ("e", "size_we", {"relx": 1, "y": 7, "rely": 0, "width": 6, "relheight": 1, "height": -14}),
        ("w", "size_we", {"x": 0, "y": 7, "relheight": 1, "height": -14, "width": 6}),
        ("nw", "size_nw_se", {"x": 0, "y": 0, "width": 10, "height": 10}),
        ("ne", "size_ne_sw", {"relx": 1, "x": -10, "y": 0, "width": 10, "height": 10}),
        ("sw", "size_ne_sw", {"x": 0, "rely": 1, "y": -10, "width": 10, "height": 10}),
        ("se", "size_nw_se", {"relx": 1, "rely": 1, "x": -10, "y": -10, "width": 10, "height": 10}),
    )

    def begin_resize(direction, event):
        state["resize"] = {
            "direction": direction,
            "pointer_x": event.x_root,
            "pointer_y": event.y_root,
            "x": root.winfo_x(),
            "y": root.winfo_y(),
            "width": root.winfo_width(),
            "height": root.winfo_height(),
        }
        return "break"

    def resize_window(event):
        drag = state["resize"]
        if not drag:
            return
        dx = event.x_root - drag["pointer_x"]
        dy = event.y_root - drag["pointer_y"]
        direction = drag["direction"]
        x, y = drag["x"], drag["y"]
        width, height = drag["width"], drag["height"]
        if "e" in direction:
            width = max(MIN_WIDTH, width + dx)
        if "s" in direction:
            height = max(MIN_HEIGHT, height + dy)
        if "w" in direction:
            new_width = max(MIN_WIDTH, width - dx)
            x += width - new_width
            width = new_width
        if "n" in direction:
            new_height = max(MIN_HEIGHT, height - dy)
            y += height - new_height
            height = new_height
        root.geometry(f"{width}x{height}+{x}+{y}")
        return "break"

    def finish_resize(_event):
        state["resize"] = None

    for direction, cursor, placement in resize_edges:
        edge = tk.Frame(root, bg=RED, cursor=cursor, bd=0)
        edge.place(**placement)
        edge.bind("<ButtonPress-1>", lambda event, d=direction: begin_resize(d, event))
        edge.bind("<B1-Motion>", resize_window)
        edge.bind("<ButtonRelease-1>", finish_resize)

    # The red labels remain fixed; only emphasized live values cycle hue.
    heading = label(content, "MiMITA DEV LOOP", size=9, bold=True)
    heading.grid(row=0, column=0, sticky="w", pady=(0, 4))
    subtitle = label(content, "Connecting to the build/runtime service…", size=8)
    subtitle.grid(row=1, column=0, sticky="ew", pady=(0, 4))

    cards = tk.Frame(content, bg=BLACK)
    cards.grid(row=2, column=0, sticky="ew")
    cards.grid_columnconfigure(0, weight=1)
    cards.grid_columnconfigure(1, weight=1)

    def make_card(title_text: str):
        card = frame(cards, padx=5, pady=3)
        card.grid_columnconfigure(0, weight=1)
        label(card, title_text.upper(), size=7, bold=True).grid(
            row=0, column=0, sticky="w")
        value = label(card, "—", size=8, bold=True, wraplength=340)
        value.grid(row=1, column=0, sticky="ew", pady=(2, 0))
        detail = label(card, "", size=7, wraplength=340)
        detail.grid(row=2, column=0, sticky="ew")
        detail.full_text = ""
        detail.short_text = ""
        return card, value, detail

    running_card, running_value, running_detail = make_card("Running build")
    latest_card, latest_value, latest_detail = make_card("Latest published")
    activity_card, phase_value, phase_detail = make_card("Build activity")
    runtime_card, runtime_value, runtime_detail = make_card("Runtime and mode/map")
    scenario_card, scenario_value, scenario_detail = make_card("Scenario evidence")

    details = frame(content, padx=5, pady=3)
    details.grid(row=3, column=0, sticky="nsew", pady=(1, 0))
    details.grid_columnconfigure(0, weight=1)
    label(details, "LATEST BUILD AND LOOP ACTIVITY  ·  LAST 10 LINES",
          size=7, bold=True).pack(anchor="w", pady=(0, 2))
    output_frame = tk.Frame(details, bg=BLACK)
    output_frame.pack(fill="both", expand=True)
    gutter = tk.Canvas(output_frame, width=28, bg=BLACK, highlightthickness=0,
                       bd=0, takefocus=False)
    gutter.pack(side="left", fill="y")
    output = tk.Text(output_frame, height=10, wrap="word", state="disabled",
                     background=BLACK, foreground=RED, insertbackground=RED,
                     selectbackground="#660000", selectforeground=WHITE,
                     relief="flat", bd=0, highlightthickness=0,
                     font=(family, 8), padx=1, pady=1)
    output.pack(side="left", fill="both", expand=True)
    output_scroll = tk.Scrollbar(output_frame, orient="vertical", command=output.yview,
                                 bg=BLACK, troughcolor=BLACK, activebackground=RED,
                                 highlightbackground=RED, bd=1)
    output_scroll.pack(side="right", fill="y")

    def update_gutter(first=None, last=None):
        if first is not None:
            output_scroll.set(first, last)
        gutter.delete("all")
        try:
            line_height = max(1, int(output.tk.call("font", "metrics", output.cget("font"), "-linespace")))
            line_height += 1
            y = 1
            while y < max(1, output.winfo_height()):
                index = output.index(f"@1,{y}")
                line_number = int(index.split(".", 1)[0])
                gutter.create_text(25, y, text=f"{line_number:02d}", anchor="ne",
                                   fill=RED, font=(family, 8))
                y += line_height
        except (tk.TclError, ValueError):
            pass

    output.configure(yscrollcommand=update_gutter)
    output.bind("<Configure>", lambda _event: update_gutter())

    action_status = label(content, "Ready", size=7)
    action_status.grid(row=4, column=0, sticky="ew", pady=(3, 1))
    if state.get("window_registration_error"):
        action_status.configure(text=state["window_registration_error"])
    elif state.get("app_id_error"):
        action_status.configure(text="Windows app identity unavailable; taskbar window still enabled")
    controls = tk.Frame(content, bg=BLACK)
    controls.grid(row=5, column=0, sticky="ew")
    controls.grid_columnconfigure(0, weight=3)
    controls.grid_columnconfigure(1, weight=1)
    controls.grid_columnconfigure(2, weight=1)

    def send(action: str) -> None:
        try:
            response = control(action)
            if isinstance(response, dict):
                accepted = bool(response.get("accepted"))
                message = response.get("message")
                request_id = str(response.get("request_id") or "")
            else:
                accepted = bool(response)
                message = None
                request_id = ""
            if accepted:
                state["pending_request_id"] = request_id
            action_status.configure(text=message or
                                    (f"{action}: queued · request {request_id}" if request_id
                                     else f"{action}: {'accepted' if accepted else 'not accepted'}"))
        except Exception as error:
            action_status.configure(text=f"{action}: {error}")
        refresh()

    launch_button = button(controls, "Launch latest stable MiMITA.exe",
                           lambda: send("build-or-launch"))
    launch_button.grid(row=0, column=0, sticky="ew", padx=(0, 3))
    button(controls, "Build", lambda: send("build")).grid(
        row=0, column=1, sticky="ew", padx=2)

    # Optional control-path smoke hook: invoke the same Tk button callback used
    # by a person, which makes dashboard launch wiring verifiable in a GUI run.
    if os.environ.pop("MIMITA_DEV_LOOP_GUI_TEST_ACTION", "") == "launch_latest":
        root.after(250, launch_button.invoke)

    options_button = button(controls, "More", lambda: show_options())
    options_button.grid(row=0, column=2, sticky="ew", padx=(3, 0))
    options_menu = tk.Menu(root, tearoff=False, bg=BLACK, fg=RED,
                           activebackground="#330000", activeforeground=WHITE,
                           bd=1, relief="raised", font=(family, 9))
    options_menu.add_command(label="Toggle game visibility", command=lambda: send("toggle-visibility"))
    options_menu.add_command(label="Toggle auto-restart", command=lambda: send("toggle-auto-restart"))
    options_menu.add_separator()

    def toggle_freeze():
        state["frozen"] = not state["frozen"]
        options_menu.entryconfigure(3, label="Resume dashboard view" if state["frozen"]
                                    else "Pause dashboard view")
        action_status.configure(text=("Dashboard paused; game and daemon continue"
                                      if state["frozen"] else "Dashboard view resumed"))
        if not state["frozen"]:
            refresh()

    options_menu.add_command(label="Pause dashboard view", command=toggle_freeze)
    options_menu.add_separator()
    options_menu.add_command(label="Stop daemon", command=lambda: send("stop"))

    def show_options():
        options_menu.tk_popup(options_button.winfo_rootx(),
                              options_button.winfo_rooty() + options_button.winfo_height())

    def rainbow_color() -> str:
        hue = ((time.monotonic() - state["hue_started"]) / 4.0) % 1.0
        r, g, b = colorsys.hsv_to_rgb(hue, 1.0, 1.0)
        return f"#{round(r * 255):02x}{round(g * 255):02x}{round(b * 255):02x}"

    def copy_build_identity(event, artifact, digest):
        text = f"{artifact}  SHA-256 {digest}"
        root.clipboard_clear()
        root.clipboard_append(text)
        action_status.configure(text="Build identity copied")
        return "break"

    for value in (running_value, latest_value):
        value.configure(cursor="hand2")

    def reflow(_event=None):
        width, height = root.winfo_width(), root.winfo_height()
        wide = width >= 760
        expanded = width >= 620 or height >= 650
        layout_changed = (wide, expanded) != (state["wide"], not state["compact"])
        state["wide"], state["compact"] = wide, not expanded
        card_wrap = max(240, (width - 36) // (2 if wide else 1) - 16)
        for _card, value, detail in (
                (running_card, running_value, running_detail),
                (latest_card, latest_value, latest_detail),
                (activity_card, phase_value, phase_detail),
                (runtime_card, runtime_value, runtime_detail),
                (scenario_card, scenario_value, scenario_detail)):
            value.configure(wraplength=card_wrap)
            detail.configure(wraplength=card_wrap)
        subtitle.configure(wraplength=max(240, width - 30))
        if not layout_changed:
            return
        for card in (running_card, latest_card, activity_card, runtime_card, scenario_card):
            card.grid_forget()
        if wide:
            running_card.grid(row=0, column=0, sticky="ew", padx=(0, 3), pady=(0, 3))
            latest_card.grid(row=0, column=1, sticky="ew", padx=(3, 0), pady=(0, 3))
            activity_card.grid(row=1, column=0, sticky="ew", padx=(0, 3), pady=(0, 3))
            runtime_card.grid(row=1, column=1, sticky="ew", padx=(3, 0), pady=(0, 3))
        else:
            running_card.grid(row=0, column=0, sticky="ew", pady=(0, 3))
            latest_card.grid(row=1, column=0, sticky="ew", pady=(0, 3))
            activity_card.grid(row=2, column=0, sticky="ew", pady=(0, 3))
            runtime_card.grid(row=3, column=0, sticky="ew", pady=(0, 3))
        if expanded:
            scenario_card.grid(row=2 if wide else 4, column=0,
                               columnspan=2 if wide else 1, sticky="ew")
        for detail in (running_detail, latest_detail):
            detail.configure(text=detail.full_text if expanded else detail.short_text)
        root.after_idle(refresh)

    def refresh() -> None:
        if not state["frozen"]:
            try:
                model = status_provider() or {}
                artifact = str(model.get("artifact_id") or "—")
                latest_hash = str(model.get("sha256") or "—")
                latest_id = str(model.get("build_id") or "—")
                running_artifact = str(model.get("running_artifact_id") or "—")
                running_hash = str(model.get("running_sha256") or "—")
                running_id = str(model.get("running_build_id") or "—")
                latest_artifact_key = _artifact_key(artifact)
                running_artifact_key = _artifact_key(running_artifact)
                same_build = bool(
                    (latest_id != "—" and latest_id == running_id)
                    or (latest_hash != "—" and latest_hash == running_hash
                        and latest_artifact_key == running_artifact_key)
                )

                running_value.configure(text=running_artifact)
                latest_value.configure(text=artifact)
                running_full = f"SHA-256 {running_hash}\nBuild ID {running_id}"
                latest_full = f"SHA-256 {latest_hash}\nBuild ID {latest_id}"
                running_short = f"SHA-256 {running_hash[:12]}…  ·  click to copy"
                latest_short = f"SHA-256 {latest_hash[:12]}…  ·  click to copy"
                running_detail.full_text, running_detail.short_text = running_full, running_short
                latest_detail.full_text, latest_detail.short_text = latest_full, latest_short
                expanded = state["wide"] or not state["compact"]
                running_detail.configure(text=running_full if expanded else running_short)
                latest_detail.configure(text=latest_full if expanded else latest_short)
                running_value.bind("<Button-1>", lambda event, a=running_artifact, h=running_hash:
                                   copy_build_identity(event, a, h))
                latest_value.bind("<Button-1>", lambda event, a=artifact, h=latest_hash:
                                  copy_build_identity(event, a, h))
                running_detail.bind("<Button-1>", lambda event, a=running_artifact, h=running_hash:
                                    copy_build_identity(event, a, h))
                latest_detail.bind("<Button-1>", lambda event, a=artifact, h=latest_hash:
                                   copy_build_identity(event, a, h))
                match_color = GREEN if same_build else rainbow_color()
                running_value.configure(fg=match_color)
                latest_value.configure(fg=match_color)
                running_detail.configure(fg=match_color)
                latest_detail.configure(fg=match_color)
                state["identity_match"] = same_build

                phase = str(model.get("phase") or "unknown").lower()
                if model.get("stale") and phase == "published_current":
                    phase_text = "SOURCE CHANGED · BUILD NEEDED"
                    phase_detail.configure(text=model.get("phase_detail") or "Current source is newer than the published build")
                    phase_color = rainbow_color()
                    state["phase_color_mode"] = "rainbow"
                elif phase == "published_current" and not model.get("identity_valid", True):
                    phase_text = "BUILD IDENTITY INVALID"
                    phase_detail.configure(text=model.get("phase_detail") or
                                           "The latest artifact identity could not be verified")
                    phase_color = RED
                    state["phase_color_mode"] = "red"
                elif phase == "published_current":
                    phase_text = "UP TO DATE"
                    phase_detail.configure(text=f"Most recent build: {artifact}")
                    phase_color = GREEN
                    state["phase_color_mode"] = "green"
                elif phase in ("building", "pending", "queued"):
                    phase_text = f"{phase.upper()}  ·  {model.get('elapsed') or '00:00:00'}"
                    phase_detail.configure(text=model.get("phase_detail") or "Build is active")
                    phase_color = rainbow_color()
                    state["phase_color_mode"] = "rainbow"
                elif "fail" in phase or "error" in phase:
                    phase_text = phase.upper()
                    phase_detail.configure(text=model.get("phase_detail") or "Build failed")
                    phase_color = RED
                    state["phase_color_mode"] = "red"
                else:
                    phase_text = phase.replace("_", " ").upper()
                    phase_detail.configure(text=model.get("phase_detail") or "Waiting for build activity")
                    phase_color = rainbow_color()
                    state["phase_color_mode"] = "rainbow"
                phase_value.configure(text=phase_text, fg=phase_color)

                runtime_value.configure(text=model.get("runtime", "Not running"),
                                         fg=GREEN if same_build else rainbow_color())
                runtime_detail.configure(text=(
                    f"{model.get('server_status', 'server unknown')}  ·  "
                    f"{model.get('client_status', 'client unknown')}  ·  "
                    f"builds {'MATCH' if same_build else 'DIFFER'}"
                ))
                mode = str(model.get("mode") or "—")
                map_name = str(model.get("map") or "—")
                mode_value.configure(text=mode, fg=rainbow_color())
                map_value.configure(text=map_name, fg=rainbow_color())

                scenario = model.get("scenario") or {}
                scenario_value.configure(
                    text=f"{scenario.get('verdict', '—')}  ·  {scenario.get('id', '—')}",
                    fg=rainbow_color())
                scenario_detail.configure(text=(
                    f"{scenario.get('state', 'No scenario recorded')}  ·  "
                    f"frame {scenario.get('frame', '—')}  ·  tick {scenario.get('tick', '—')}  ·  "
                    f"checkpoint {scenario.get('checkpoint', '—')} "
                    f"{scenario.get('checkpoint_progress', '')}  ·  "
                    f"capture {scenario.get('complete', 'unknown')}\n"
                    f"map {scenario.get('map', '—')}  ·  "
                    f"build {scenario.get('build_id', latest_id)}\n"
                    f"journal {model.get('journal') or '(none)'}\n"
                    f"{scenario.get('log_usage', '')}"
                ))

                subtitle.configure(text=(
                    f"daemon {model.get('daemon_pid', '—')}  ·  "
                    f"updated {model.get('updated_at', '—')}"
                ))
                build_lines = [f"[BUILD] {line}" for line in (model.get("build_output") or [])[-10:]]
                activity_lines = [f"[LOOP] {line}" for line in (model.get("recent") or [])[-10:]]
                source_line = model.get("source_activity")
                if source_line:
                    activity_lines.append(f"[SOURCE] {source_line}")
                lines = (build_lines[-7:] + activity_lines[-3:])[-10:]
                if not lines:
                    lines = ["Waiting for build or contributor activity…"]
                output.configure(state="normal")
                output.delete("1.0", "end")
                output.insert("1.0", "\n".join(lines))
                output.configure(state="disabled")
                output.yview_moveto(1.0)
                update_gutter()
                last_control = model.get("last_control") or {}
                pending_request = state.get("pending_request_id")
                if (pending_request and last_control.get("request_id") == pending_request
                        and last_control.get("status") in ("completed", "failed", "rejected")):
                    action_status.configure(text=last_control.get("message") or
                                             f"{last_control.get('action', 'action')}: "
                                             f"{last_control.get('status')}")
                    state["pending_request_id"] = ""
            except Exception as error:
                action_status.configure(text=f"Status refresh failed: {error}")
        if state["after_id"] is not None:
            try:
                root.after_cancel(state["after_id"])
            except tk.TclError:
                pass
        state["after_id"] = root.after(1000, refresh)

    def animate_values():
        if not state["frozen"] and not state["minimized"]:
            color = rainbow_color()
            identity_color = GREEN if state["identity_match"] else color
            running_value.configure(fg=identity_color)
            latest_value.configure(fg=identity_color)
            running_detail.configure(fg=identity_color)
            latest_detail.configure(fg=identity_color)
            phase_mode = state["phase_color_mode"]
            phase_value.configure(fg=(GREEN if phase_mode == "green" else
                                      RED if phase_mode == "red" else color))
            runtime_value.configure(fg=identity_color)
            mode_value.configure(fg=color)
            map_value.configure(fg=color)
            scenario_value.configure(fg=color)
        state["animation_id"] = root.after(60, animate_values)

    mode_map = tk.Frame(runtime_card, bg=BLACK)
    mode_map.grid(row=3, column=0, sticky="ew")
    label(mode_map, "Mode:", size=7).grid(row=0, column=0, sticky="w")
    mode_value = label(mode_map, "—", size=7, bold=True)
    mode_value.grid(row=0, column=1, sticky="w", padx=(3, 12))
    label(mode_map, "Map:", size=7).grid(row=0, column=2, sticky="w")
    map_value = label(mode_map, "—", size=7, bold=True)
    map_value.grid(row=0, column=3, sticky="w", padx=(3, 0))

    root.bind("<Configure>", reflow, add="+")
    def on_map(_event):
        state["minimized"] = False
        if os.name == "nt":
            root.after_idle(mark_as_app_window)

    root.bind("<Map>", on_map, add="+")
    root.bind("<Key-1>", lambda _event: send("build-or-launch"))
    root.bind("<Key-b>", lambda _event: send("build"))
    root.bind("<Key-v>", lambda _event: send("toggle-visibility"))
    root.bind("<Key-a>", lambda _event: send("toggle-auto-restart"))
    root.bind("<Key-p>", lambda _event: toggle_freeze())
    root.bind("<Key-x>", lambda _event: close_view())

    def cleanup():
        for key in ("after_id", "animation_id"):
            if state[key] is not None:
                try:
                    root.after_cancel(state[key])
                except tk.TclError:
                    pass
        if font_library:
            try:
                font_library.RemoveFontResourceExW.argtypes = [ctypes.c_wchar_p,
                                                               ctypes.c_uint,
                                                               ctypes.c_void_p]
                font_library.RemoveFontResourceExW.restype = ctypes.c_int
                font_library.RemoveFontResourceExW(str(FONT_PATH), 0x10, None)
            except (AttributeError, OSError):
                pass

    root.protocol("WM_DELETE_WINDOW", close_view)
    root.bind("<Destroy>", lambda event: cleanup() if event.widget is root else None, add="+")
    reflow()
    refresh()
    animate_values()
    root.mainloop()
    return 0
