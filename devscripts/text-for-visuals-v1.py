import os
import random
import subprocess
from PIL import Image, ImageDraw, ImageFont

# =========================
# CONFIG
# =========================
WIDTH = 666
HEIGHT = 666
FPS = 60
DURATION_SECONDS = 10

TEXT = "TEST TEXT"

FONT_PATH = r"C:\important\mingliu-font\mingliu.ttf"

OUTPUT_VIDEO = "color_explosion.mp4"
FRAME_DIR = "_temp_frames"

# =========================

os.makedirs(FRAME_DIR, exist_ok=True)

TOTAL_FRAMES = FPS * DURATION_SECONDS

def get_max_font_size(text, font_path, width, height):
    size = 10

    while True:
        font = ImageFont.truetype(font_path, size)

        dummy = Image.new("RGB", (1, 1))
        draw = ImageDraw.Draw(dummy)

        bbox = draw.textbbox((0, 0), text, font=font)

        w = bbox[2] - bbox[0]
        h = bbox[3] - bbox[1]

        if w > width * 0.98 or h > height * 0.98:
            return max(size - 1, 10)

        size += 4

print("Finding maximum font size...")

FONT_SIZE = get_max_font_size(
    TEXT,
    FONT_PATH,
    WIDTH,
    HEIGHT
)

print(f"Using font size: {FONT_SIZE}")

font = ImageFont.truetype(FONT_PATH, FONT_SIZE)

print(f"Generating {TOTAL_FRAMES} frames...")

for frame in range(TOTAL_FRAMES):

    bg_color = (
        random.randint(0, 255),
        random.randint(0, 255),
        random.randint(0, 255)
    )

    text_color = (
        random.randint(0, 255),
        random.randint(0, 255),
        random.randint(0, 255)
    )

    img = Image.new("RGB", (WIDTH, HEIGHT), bg_color)
    draw = ImageDraw.Draw(img)

    bbox = draw.textbbox((0, 0), TEXT, font=font)

    text_w = bbox[2] - bbox[0]
    text_h = bbox[3] - bbox[1]

    x = (WIDTH - text_w) // 2
    y = (HEIGHT - text_h) // 2

    draw.text(
        (x, y),
        TEXT,
        font=font,
        fill=text_color
    )

    img.save(
        os.path.join(
            FRAME_DIR,
            f"frame_{frame:06d}.png"
        )
    )

    if frame % 100 == 0:
        print(f"{frame}/{TOTAL_FRAMES}")

print("Encoding MP4...")

subprocess.run([
    "ffmpeg",
    "-y",
    "-framerate", str(FPS),
    "-i", os.path.join(FRAME_DIR, "frame_%06d.png"),
    "-c:v", "libx264",
    "-crf", "18",
    "-preset", "fast",
    "-pix_fmt", "yuv420p",
    OUTPUT_VIDEO
], check=True)

print("Cleaning up PNGs...")

for file in os.listdir(FRAME_DIR):
    os.remove(os.path.join(FRAME_DIR, file))

os.rmdir(FRAME_DIR)

print()
print("DONE")
print("Video saved to:")
print(os.path.abspath(OUTPUT_VIDEO))