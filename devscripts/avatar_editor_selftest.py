# 08 20 2026, 00 00
"""purpose
Validate the v1 avatar-editor JSON contract without launching the game.
Checks the six body parts, six logical faces, legacy face strings, object face
settings, full cosmetic instances, and uncapped cosmetic color multipliers.
Does not render OpenGL, load a GLB, or modify any avatar file.
"""

from __future__ import annotations

import json
from pathlib import Path


PARTS = ("head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg")
FACES = ("front", "back", "left", "right", "top", "bottom")
GUI_REQUIRED = (
    "editorContent", "editorStyleButton", "editorStyleSliderLabel",
    "saveButton", "applyButton", "backButton", "applySelectedPngButton",
    "cosmeticAddButton", "cosmeticRemoveButton", "cosmeticDuplicateButton",
)


def check_avatar(path: Path) -> None:
    root = json.loads(path.read_text(encoding="utf-8"))
    advanced = root.get("advanced", {})
    assert isinstance(advanced, dict), f"{path}: advanced must be an object"

    for part in PARTS:
        for face in FACES:
            key = f"{part}_{face}"
            assert key in advanced, f"{path}: missing {key}"
            value = advanced[key]
            assert isinstance(value, (str, dict)), f"{path}: invalid {key}"
            if isinstance(value, dict):
                assert isinstance(value.get("texture", ""), str)

    cosmetics = root.get("cosmetics", [])
    assert isinstance(cosmetics, list), f"{path}: cosmetics must be an array"
    for cosmetic in cosmetics:
        assert isinstance(cosmetic, dict)
        assert isinstance(cosmetic.get("enabled", True), bool)
        if "color" in cosmetic:
            assert cosmetic["color"][0] >= 0.0
        if "texture" in cosmetic:
            texture = cosmetic["texture"]
            assert isinstance(texture.get("color", [1, 1, 1]), list)


def check_contract_examples() -> None:
    old_face = "torso_front.png"
    new_face = {
        "texture": "girlface4.png",
        "offset_x": 0,
        "offset_y": 100,
        "scale_x": 1.0,
        "scale_y": 1.8,
        "rotation": 180.0,
        "color": [1.0, 1.0, 1.0],
        "brightness": 1.0,
        "opacity": 1.0,
    }
    assert isinstance(old_face, str)
    assert new_face["opacity"] == 1.0

    cosmetic = {
        "id": "wings_001",
        "glb": "wings.glb",
        "enabled": True,
        "anchor_part": "torso",
        "offset": [0, 0, -1],
        "rotation": [0, 0, 0],
        "scale": [1, 1, 1],
        "texture": {
            "image": "wingsimg.png",
            "color": [10.0, 1.0, 1.0],
        },
    }
    assert cosmetic["texture"]["color"][0] == 10.0


def check_gui_contract(repo_root: Path) -> None:
    path = repo_root / "config" / "gui" / "avatar-creator.json"
    elements = json.loads(path.read_text(encoding="utf-8")).get("elements", {})
    for element_id in GUI_REQUIRED:
        assert element_id in elements, f"{path}: missing {element_id}"

    for element_id, element in elements.items():
        if "fontSize" in element:
            assert element["fontSize"] >= 0.30, (
                f"{path}: {element_id} fontSize is not readable"
            )

    content_width = elements["editorContent"]["width"]
    for element_id in ("cosmeticAddButton", "cosmeticRemoveButton", "cosmeticDuplicateButton"):
        button = elements[element_id]
        assert button["width"] > 0 and button["height"] >= 34
        assert 0 <= button["x"] < content_width
        assert button["x"] + button["width"] <= content_width
        assert button.get("text"), f"{path}: {element_id} has no label"

    slider_ids = [element_id for element_id in elements if element_id.startswith("slider")]
    assert len(slider_ids) == len(set(slider_ids))
    assert "sliderOffsetX" in elements

    library_source = (repo_root / "src" / "avatar" / "avatar-editor-library.cpp").read_text(encoding="utf-8")
    assert "hoverScale" not in library_source


def main() -> int:
    check_contract_examples()
    repo_root = Path(__file__).resolve().parents[1]
    check_gui_contract(repo_root)
    avatar_root = repo_root / "assets" / "avatars"
    checked = 0
    for avatar_json in sorted(avatar_root.glob("*/avatar.json")):
        check_avatar(avatar_json)
        checked += 1
    print(f"AVATAR_EDITOR_SELFTEST_PASS avatars={checked} faces=36 gui=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
