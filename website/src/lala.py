from PIL import Image
import os

FILES = [

    # # main logos
    # r"C:\important\branding-jorj1357-0mastery\fake raptv post jorj rav maybe.png JORJ LOGO v8.png",
    # r"C:\important\branding-jorj1357-0mastery\3cage pfp v2.png",
    # r"C:\important\branding-jorj1357-0mastery\ids logo v1.png",

    # # website icons
    # r"C:\important\mimita-priv-v8\website\public\tiktok icon v1.png",
    # r"C:\important\mimita-priv-v8\website\public\instagram icon v1.png",
    # r"C:\important\mimita-priv-v8\website\public\spotify logo v1.png",
    # r"C:\important\mimita-priv-v8\website\public\discord icon v1.png",
    # r"C:\important\mimita-priv-v8\website\public\youtube icon v1.png",
    r"C:\Users\guita\OneDrive\Desktop\get teh burger thumb v1.png",
]

MAX_SIZE = 1024

for path in FILES:

    if not os.path.exists(path):
        print(f"missing: {path}")
        continue

    img = Image.open(path)

    # preserve transparency
    if img.mode not in ("RGBA", "LA"):
        img = img.convert("RGBA")

    width, height = img.size

    # resize only if larger than max
    if width > MAX_SIZE or height > MAX_SIZE:

        scale = min(MAX_SIZE / width, MAX_SIZE / height)

        new_width = int(width * scale)
        new_height = int(height * scale)

        img = img.resize(
            (new_width, new_height),
            Image.LANCZOS
        )

    base, ext = os.path.splitext(path)

    output = f"{base}-optimized.webp"

    img.save(
        output,
        format="WEBP",
        quality=92,
        method=6
    )

    old_size = os.path.getsize(path) / 1024
    new_size = os.path.getsize(output) / 1024

    print()
    print(f"done: {os.path.basename(path)}")
    print(f"old: {old_size:.1f}kb")
    print(f"new: {new_size:.1f}kb")
    print(f"saved: {output}")

print()
print("finished")
