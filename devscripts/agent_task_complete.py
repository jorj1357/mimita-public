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
        winsound.PlaySound(sound_path, winsound.SND_ASYNC | winsound.SND_NODEFAULT)
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

    play_sound()
    show_notification(task_name)


if __name__ == "__main__":
    main()
