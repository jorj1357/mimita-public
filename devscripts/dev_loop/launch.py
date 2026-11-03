"""Published executable selection and explicit stable-build fallback."""

from __future__ import annotations

from pathlib import Path

from .artifacts import identity_for_folder, published_builds


def select_launch_artifact(latest_folder: str | None, build_root: Path,
                           executable_identity) -> dict:
    identity = (identity_for_folder(build_root / latest_folder, executable_identity)
                if latest_folder else None)
    if identity and identity.get("valid"):
        return {"folder": latest_folder, "identity": identity,
                "fallback": False, "validation_error": ""}

    validation_error = ""
    if latest_folder:
        executable_check = executable_identity(build_root / latest_folder / "mimita.exe")
        validation_error = (
            str(executable_check.get("validation") or "executable invalid")
            if not executable_check.get("valid") else
            "artifact identity sidecar is missing or mismatched"
        )

    for candidate in published_builds(build_root):
        candidate_identity = identity_for_folder(candidate, executable_identity)
        if candidate_identity and candidate_identity.get("valid"):
            return {
                "folder": candidate.name,
                "identity": candidate_identity,
                "fallback": candidate.name != latest_folder,
                "validation_error": validation_error,
            }
    return {"folder": None, "identity": None, "fallback": False,
            "validation_error": validation_error}
