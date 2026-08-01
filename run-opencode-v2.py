import subprocess
from pathlib import Path

# Folder containing this Python script
folder = Path(__file__).resolve().parent

subprocess.Popen(
    [
        "pwsh",
        "-NoExit",
        "-Command",
        f"Set-Location -LiteralPath '{folder}'; opencode"
    ],
    cwd=folder,
)