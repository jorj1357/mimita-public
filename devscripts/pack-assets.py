"""Pack assets/ directory into assets.pak (single-file archive)."""

import json
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PAK_MAGIC = b"PAK1"

def pack_assets():
    assets_dir = os.path.join(ROOT, "assets")
    pak_path = os.path.join(ROOT, "assets.pak")

    # Collect all asset files
    files = []
    total_size = 0
    for dirpath, dirnames, filenames in os.walk(assets_dir):
        for fn in sorted(filenames):
            full = os.path.join(dirpath, fn)
            rel = os.path.relpath(full, ROOT).replace("\\", "/")
            size = os.path.getsize(full)
            files.append((rel, full, size))
            total_size += size

    print(f"Packing {len(files)} files ({total_size / 1e6:.1f} MB)...")

    # Build directory: list of (path, offset, size)
    # First pass: compute offsets
    header_size = 4 + 4  # magic + num_files
    dir_offset = header_size
    toc_entries = []
    data_offset = dir_offset

    for rel, full, size in files:
        entry_payload = 2 + len(rel) + 4 + 4 + 4  # path_len + path + data_len + offset_marker + size_marker
        data_offset += 4 + entry_payload  # each entry has a 4-byte entry_size prefix

    # Actually let me make the format simpler. Just: [magic][num_files][entries...][data...]
    # Each entry: [path_len:2][path:path_len][data_len:4][data:data_len]
    # Simple concatenation.

    with open(pak_path, "wb") as pak:
        pak.write(PAK_MAGIC)  # 4 bytes magic
        pak.write(struct.pack("<I", len(files)))  # 4 bytes num_files

        # Write table of contents (offsets will be filled after data is written)
        toc = []
        data_start = 4 + 4  # after magic + num_files

        # Calculate TOC size
        toc_size = 0
        for rel, full, size in files:
            path_bytes = rel.encode("utf-8")
            entry_header_size = 2 + len(path_bytes) + 4 + 8 + 8  # path_len + path + data_len + offset + size
            toc_size += entry_header_size

        data_start += toc_size

        # Write TOC with placeholder data_start
        current_offset = data_start
        toc_entries = []
        for rel, full, size in files:
            path_bytes = rel.encode("utf-8")
            toc_entries.append((path_bytes, current_offset, size))
            current_offset += size

        # Write actual TOC
        for path_bytes, offset, size in toc_entries:
            pak.write(struct.pack("<H", len(path_bytes)))
            pak.write(path_bytes)
            pak.write(struct.pack("<Q", offset))
            pak.write(struct.pack("<Q", size))

        # Write file data
        written = 0
        for rel, full, size in files:
            with open(full, "rb") as f:
                data = f.read()
            pak.write(data)
            written += 1
            if written % 50 == 0:
                print(f"  {written}/{len(files)} files packed")

    pak_size = os.path.getsize(pak_path)
    print(f"\n[OK] Packed {len(files)} files -> {pak_path}")
    print(f"     Input:  {total_size / 1e6:.1f} MB")
    print(f"     Output: {pak_size / 1e6:.1f} MB")
    print(f"     Ratio:  {pak_size / total_size * 100:.1f}%")

if __name__ == "__main__":
    pack_assets()
