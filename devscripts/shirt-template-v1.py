from PIL import Image, ImageDraw, ImageFont

# =========================
# CONFIG
# =========================

IMG_SIZE = 2000
BACKGROUND = (0, 0, 0)
LINE = (255, 255, 255)
TEXT = (255, 255, 255)

PARTS = [
    "HEAD",
    "TORSO",
    "LEFT_ARM",
    "RIGHT_ARM",
    "LEFT_LEG",
    "RIGHT_LEG",
]

SIDES = [
    "TOP",
    "BOTTOM",
    "FRONT",
    "BACK",
    "LEFT",
    "RIGHT",
]

GRID_COLS = 6
GRID_ROWS = 6

PADDING = 40
CELL_GAP = 8

# =========================
# CREATE IMAGE
# =========================

img = Image.new("RGB", (IMG_SIZE, IMG_SIZE), BACKGROUND)
draw = ImageDraw.Draw(img)

usable_w = IMG_SIZE - (PADDING * 2)
usable_h = IMG_SIZE - (PADDING * 2)

cell_w = (usable_w - (CELL_GAP * (GRID_COLS - 1))) // GRID_COLS
cell_h = (usable_h - (CELL_GAP * (GRID_ROWS - 1))) // GRID_ROWS

# =========================
# FONT
# =========================

try:
    font = ImageFont.truetype("arial.ttf", 22)
except:
    font = ImageFont.load_default()

# =========================
# DRAW GRID
# =========================

index = 0

for part in PARTS:
    for side in SIDES:

        row = index // GRID_COLS
        col = index % GRID_COLS

        x0 = PADDING + col * (cell_w + CELL_GAP)
        y0 = PADDING + row * (cell_h + CELL_GAP)

        x1 = x0 + cell_w
        y1 = y0 + cell_h

        # 1px white border
        draw.rectangle(
            [x0, y0, x1, y1],
            outline=LINE,
            width=1
        )

        label = f"{part}_{side}"

        # text size
        bbox = draw.textbbox((0, 0), label, font=font)
        tw = bbox[2] - bbox[0]
        th = bbox[3] - bbox[1]

        tx = x0 + (cell_w - tw) / 2
        ty = y0 + (cell_h - th) / 2

        draw.text((tx, ty), label, fill=TEXT, font=font)

        index += 1

# =========================
# SAVE
# =========================

output = "character_outfit_template.png"
img.save(output)

print(f"saved: {output}")