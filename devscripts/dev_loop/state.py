"""Durable JSON state-file IO used by the dev-loop daemon."""

from __future__ import annotations

import errno
import json
import os
import tempfile
import threading
import time
from pathlib import Path

from jsonc import load as load_jsonc


_JSON_WRITE_LOCK = threading.Lock()
_JSON_REPLACE_RETRIES = 8


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = json.dumps(value, indent=2) + "\n"
    temporary = None
    with _JSON_WRITE_LOCK:
        try:
            with tempfile.NamedTemporaryFile(
                mode="w", encoding="utf-8", newline="\n",
                dir=path.parent, prefix=f".{path.name}.", suffix=".tmp",
                delete=False,
            ) as handle:
                temporary = Path(handle.name)
                handle.write(payload)
                handle.flush()
                os.fsync(handle.fileno())

            for attempt in range(_JSON_REPLACE_RETRIES):
                try:
                    os.replace(temporary, path)
                    temporary = None
                    break
                except PermissionError as error:
                    winerror = getattr(error, "winerror", None)
                    retryable = winerror in (5, 32, 33) or error.errno in (
                        errno.EACCES, errno.EBUSY, errno.EPERM,
                    )
                    if not retryable or attempt + 1 == _JSON_REPLACE_RETRIES:
                        raise
                    time.sleep(0.025 * (attempt + 1))
        finally:
            if temporary is not None:
                try:
                    temporary.unlink()
                except FileNotFoundError:
                    pass


def read_json(path: Path) -> dict:
    return load_jsonc(path)
