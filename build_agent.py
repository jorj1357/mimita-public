# build_agent.py
# Build script for AI agents.
# Builds without running the exe, so agents never hang.

import subprocess
import sys

if __name__ == "__main__":
    result = subprocess.run([sys.executable, "build.py", "build-only"])
    sys.exit(result.returncode)
