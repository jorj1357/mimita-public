import re, pathlib

root = pathlib.Path(r"C:\important\go away v5\s\mimita-v5\src")
targets = ["renderer.cpp", "Renderer.cpp", "main.cpp"]

# Map old immediate mode functions to comments (so you can replace manually)
legacy_calls = [
    "glBegin", "glEnd", "glVertex3f", "glColor3f", "glMatrixMode",
    "glLoadMatrixf", "glTranslatef", "glPushMatrix", "glPopMatrix"
]

for file in targets:
    path = next(root.rglob(file), None)
    if not path: continue

    code = path.read_text()

    # Comment out all immediate mode calls so build won't fail
    for call in legacy_calls:
        code = re.sub(rf"\b{call}\b", f"// LEGACY_REMOVED({call})", code)

    # Add hint comment where to use new shader/VBO code
    if "drawCube" in code:
        code = "// TODO: Replace with VAO/VBO + shader pipeline\n" + code

    path.write_text(code)
    print(f"✅ Refactored: {path}")
