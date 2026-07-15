"""
Agent task completion hook.

Usage:
    python devscripts/agent_task_complete.py [task_name]

Requirements:
    Python 3 (standard library only, no pip deps)
"""

import sys
import subprocess
import os

# ── Config ──────────────────────────────────────────────────────────
SOUND_VOLUME = 0.1  # 1 = full volume, 0 = full mute
# ────────────────────────────────────────────────────────────────────


def release_locks():
    """Release all held locks via the coordination system."""
    try:
        # Import here so the script works even if coordination module has issues
        sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        from devscripts.agent_coordination import AgentCoordinator, _read_status

        data = _read_status()
        if not data:
            return

        # Find own agent entry by PID and release all its locks
        my_pid = os.getpid()
        for agent_id, info in data.items():
            if info.get("pid") == my_pid or info.get("agentId", "").endswith(str(my_pid)):
                locks = info.get("locks", [])
                if locks:
                    coord = AgentCoordinator("completion_hook", agent_id=agent_id)
                    for lock in list(locks):
                        coord.release_lock(lock)
                    coord.update_state("DONE", details="Agent task completed")
                return
    except Exception as e:
        print(f"[AGENT WARNING] Lock release failed: {e}")


def play_sound():
    sound_path = os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
        "assets", "sound", "entity", "player", "spawning.wav"
    )
    if not os.path.exists(sound_path):
        print(f"[AGENT WARNING] Sound file not found: {sound_path}")
        return

    try:
        import winsound
        import struct
        import wave
        import tempfile

        # Read WAV, scale samples by SOUND_VOLUME, write to temp file
        with wave.open(sound_path, 'rb') as wf:
            params = wf.getparams()
            frames = wf.readframes(wf.getnframes())

        if SOUND_VOLUME < 1.0 and params.sampwidth == 2:
            samples = struct.unpack_from(f'<{len(frames) // 2}h', frames)
            scaled = [max(-32768, min(32767, int(s * SOUND_VOLUME))) for s in samples]
            frames = struct.pack(f'<{len(scaled)}h', *scaled)
        elif SOUND_VOLUME < 1.0 and params.sampwidth == 1:
            samples = struct.unpack_from(f'<{len(frames)}B', frames)
            scaled = [max(0, min(255, int((s - 128) * SOUND_VOLUME) + 128)) for s in samples]
            frames = struct.pack(f'<{len(scaled)}B', *scaled)

        with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as tmp:
            with wave.open(tmp, 'wb') as wf:
                wf.setparams(params)
                wf.writeframes(frames)
            temp_path = tmp.name

        winsound.PlaySound(temp_path, winsound.SND_NODEFAULT)
        os.unlink(temp_path)
    except Exception as e:
        print(f"[AGENT WARNING] Sound playback failed: {e}")


def show_notification(task_name):
    title = "MiMITA Agent"
    message = "Task Completed"
    if task_name:
        message = f"Task Completed: {task_name}"

    ps_script = f'''
    [Windows.UI.Notifications.ToastNotificationManager, Windows.UI.Notifications, ContentType = WindowsRuntime] > $null
    $template = [Windows.UI.Notifications.ToastNotificationManager]::GetTemplateContent([Windows.UI.Notifications.ToastTemplateType]::ToastText02)
    $textNodes = $template.GetElementsByTagName("text")
    $textNodes.Item(0).AppendChild($template.CreateTextNode("{title}")) > $null
    $textNodes.Item(1).AppendChild($template.CreateTextNode("{message}")) > $null
    $toast = [Windows.UI.Notifications.ToastNotification]::new($template)
    [Windows.UI.Notifications.ToastNotificationManager]::CreateToastNotifier("MiMITA Agent").Show($toast)
    '''

    try:
        result = subprocess.run(
            ["powershell", "-NoProfile", "-Command", ps_script],
            capture_output=True, text=True, timeout=10
        )
        if result.returncode != 0:
            stderr = result.stderr.strip()
            if stderr:
                print(f"[AGENT WARNING] Notification failed: {stderr}")
    except Exception as e:
        print(f"[AGENT WARNING] Notification exception: {e}")


def main():
    task_name = " ".join(sys.argv[1:]) if len(sys.argv) > 1 else ""

    print("[AGENT COMPLETE]")
    print("Task finished successfully")
    if task_name:
        print(f"Task: {task_name}")

    release_locks()
    play_sound()
    show_notification(task_name)


if __name__ == "__main__":
    main()
