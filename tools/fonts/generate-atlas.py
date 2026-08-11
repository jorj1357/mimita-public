#!/usr/bin/env python3
# 08 11 2026, 12 00
"""
purpose
* Regenerate MiMITA's bitmap UI font atlas (.fnt text descriptor + PNG pages)
  from Noto Serif CJK TC, reproducing the old MingLiU atlas settings:
  size=48, lineHeight=48, base=38, 256x256 pages, padding 0, spacing 1.
* This script DOES NOT ship the source .otf, touch game code, or change gameplay.
* This script DOES NOT modify anything outside assets/font/.
* The game renders one byte per glyph (Latin-1), so only the 140 old codepoints
  are generated; control chars are blank placeholders.

Dev dependency (not a game dependency):
    pip install freetype-py

Source font (SIL Open Font License 1.1), kept next to this script:
    NotoSerifCJKtc-Regular.otf
    Download: https://github.com/googlefonts/noto-cjk/releases  (Serif2TC.zip)

Usage:
    python tools/fonts/generate-atlas.py
"""

import os
import sys

try:
    import freetype
except ImportError:
    sys.exit("freetype-py is required:  pip install freetype-py")

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
FONT_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "NotoSerifCJKtc-Regular.otf")
OUT_DIR = os.path.join(ROOT, "assets", "font")
BASE_NAME = "noto-serif-cjk-tc-mimita-v1"

# Same codepoints the old mingliu-mimita-v3.fnt shipped (chars count=140):
# 1..126 plus the Latin-1 supplement glyphs that had full-width advances.
CHAR_IDS = list(range(1, 127)) + [
    162, 163, 165, 167, 168, 175, 176, 177, 180, 181, 183, 184, 215, 247,
]

FONT_SIZE = 48
LINE_HEIGHT = 48
BASE = 38
PAGE = 256
GAP = 1  # matches old spacing=1,1 so glyphs never bleed into neighbors


def main():
    if not os.path.isfile(FONT_PATH):
        sys.exit(
            f"Missing source font:\n  {FONT_PATH}\n"
            "Download NotoSerifCJKtc-Regular.otf into tools/fonts/ "
            "(https://github.com/googlefonts/noto-cjk/releases, Serif2TC.zip) "
            "then re-run."
        )

    face = freetype.Face(FONT_PATH)
    face.set_char_size(FONT_SIZE * 64)

    # Each entry: (id, w, h, xoffset, yoffset, xadvance, rows, pitch)
    packed = []
    for cp in CHAR_IDS:
        if face.get_char_index(cp) == 0:
            packed.append((cp, 1, 1, 0, 0, 24, None, 0))
            continue
        face.load_char(cp, freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_NORMAL)
        g = face.glyph
        b = g.bitmap
        adv = round(g.advance.x / 64.0) or 24
        if b.width == 0 or b.rows == 0:
            packed.append((cp, 1, 1, 0, 0, adv, None, 0))
            continue
        xoff = g.bitmap_left
        yoff = BASE - g.bitmap_top
        packed.append((cp, b.width, b.rows, xoff, yoff, adv, b.buffer, b.pitch))

    pages = []  # list of dicts: {"img": [alpha bytearray], "map": {cp: (x, y)}}

    def add_page():
        pages.append({"img": bytearray(PAGE * PAGE), "map": {}})

    add_page()
    row_y = 0
    row_x = 0
    row_h = 0

    for entry in sorted(packed, key=lambda e: -e[2]):
        cp, w, h, xoff, yoff, adv, rows, pitch = entry
        if row_x + w + GAP > PAGE:
            row_x = 0
            row_y += row_h + GAP
            row_h = 0
        if row_y + h + GAP > PAGE:
            add_page()
            row_x = 0
            row_y = 0
            row_h = 0
        x, y = row_x, row_y
        if rows is not None:
            img = pages[-1]["img"]
            for r in range(h):
                src = rows[r * pitch:(r + 1) * pitch]
                dst = (y + r) * PAGE + x
                img[dst:dst + w] = bytes(src)
        pages[-1]["map"][cp] = (x, y)
        row_x += w + GAP
        row_h = max(row_h, h)

    page_of = {}
    for i, pg in enumerate(pages):
        for cp in pg["map"]:
            page_of[cp] = i

    os.makedirs(OUT_DIR, exist_ok=True)
    fnt_path = os.path.join(OUT_DIR, BASE_NAME + ".fnt")
    with open(fnt_path, "w", encoding="ascii", newline="\n") as f:
        f.write('info face="Noto Serif CJK TC" size=%d bold=0 italic=0 charset="" unicode=1 '
                'stretchH=100 smooth=1 aa=1 padding=0,0,0,0 spacing=1,1 outline=0\n' % FONT_SIZE)
        f.write("common lineHeight=%d base=%d scaleW=%d scaleH=%d pages=%d packed=0 "
                "alphaChnl=1 redChnl=0 greenChnl=0 blueChnl=0\n"
                % (LINE_HEIGHT, BASE, PAGE, PAGE, len(pages)))
        for i in range(len(pages)):
            f.write('page id=%d file="%s_%d.png"\n' % (i, BASE_NAME, i))
        f.write("chars count=%d\n" % len(packed))
        for cp, w, h, xoff, yoff, adv, _, _ in packed:
            x, y = pages[page_of[cp]]["map"][cp]
            f.write(
                "char id=%-4d x=%-4d y=%-4d width=%-3d height=%-3d xoffset=%-3d "
                "yoffset=%-3d xadvance=%-3d page=%d chnl=15\n"
                % (cp, x, y, w, h, xoff, yoff, adv, page_of[cp])
            )

    for i, pg in enumerate(pages):
        alpha = Image.frombytes("L", (PAGE, PAGE), bytes(pg["img"]))
        img = Image.new("RGBA", (PAGE, PAGE), (255, 255, 255, 255))
        img.putalpha(alpha)
        img.save(os.path.join(OUT_DIR, "%s_%d.png" % (BASE_NAME, i)))

    print("[OK] %s" % fnt_path)
    for i in range(len(pages)):
        print("[OK] %s" % os.path.join(OUT_DIR, "%s_%d.png" % (BASE_NAME, i)))
    print("[OK] chars=%d pages=%d size=%dx%d lineHeight=%d base=%d"
          % (len(packed), len(pages), PAGE, PAGE, LINE_HEIGHT, BASE))


if __name__ == "__main__":
    main()
