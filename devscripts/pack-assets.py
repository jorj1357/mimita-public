"""Pack assets/ directory into assets.pak (single-file archive).
Format: [magic:4][num_files:4][entries...][data...]
Each entry: [path_len:2][path:path_len][offset:8][size:8]
"""

import os
import struct

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PAK_MAGIC = b"PAK1"

def pack_assets():
    assets_dir = os.path.join(ROOT, "assets")
    pak_path = os.path.join(ROOT, "assets.pak")

    files = []
    total_size = 0
    for dirpath, dirnames, filenames in os.walk(assets_dir):
        for fn in sorted(filenames):
            full = os.path.join(dirpath, fn)
            rel = os.path.relpath(full, ROOT).replace("\\", "/")
            size = os.path.getsize(full)
            files.append((rel.encode("utf-8"), full, size))
            total_size += size

    print(f"Packing {len(files)} files ({total_size / 1e6:.1f} MB)...")

    # Header: magic + num_files
    data_start = 4 + 4

    # TOC size: each entry has path_len(2) + path + offset(8) + size(8)
    toc_size = 0
    for path_bytes, full, sz in files:
        toc_size += 2 + len(path_bytes) + 8 + 8
    data_start += toc_size

    # Compute offsets
    current_offset = data_start
    toc = []
    for path_bytes, full, sz in files:
        toc.append((path_bytes, current_offset, sz))
        current_offset += sz

    with open(pak_path, "wb") as pak:
        pak.write(PAK_MAGIC)
        pak.write(struct.pack("<I", len(files)))

        for path_bytes, offset, sz in toc:
            pak.write(struct.pack("<H", len(path_bytes)))
            pak.write(path_bytes)
            pak.write(struct.pack("<Q", offset))
            pak.write(struct.pack("<Q", sz))

        written = 0
        for path_bytes, full, sz in files:
            with open(full, "rb") as f:
                pak.write(f.read())
            written += 1
            if written % 50 == 0:
                print(f"  {written}/{len(files)}")

    s = os.path.getsize(pak_path)
    print(f"[OK] {len(files)} files -> {s / 1e6:.1f} MB ({s / total_size * 100:.1f}%)")

if __name__ == "__main__":
    pack_assets()
