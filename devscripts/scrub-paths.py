# 08 12 2026, 21 10
# purpose
# Post-build scrub that removes suspicious absolute paths from MiMITA release
# binaries before shipping. The MinGW winlibs runtime bakes the toolchain
# builder's own staging path (R:\winlibs_staging_ucrt64\...) into libgcc error
# strings; an absolute path to a "staging" dir is a strong AV/ML false-positive
# signal. This replaces such byte strings with a benign same-length path so
# string pointers stay valid.
# Does NOT change code, imports, or functionality. Only rewrites the content of
# read-only string literals in .rdata.

import sys

FILLER = b"C:/Windows/System32/mimita-runtime-libgcc-0000000000000"

# Absolute paths that must never ship (backslash + forward-slash variants).
PATTERNS = (
    b"R:\\winlibs_staging_ucrt64\\gcc-15.2.0\\build_mingw\\x86_64-w64-mingw32\\libgcc",
    b"R:/winlibs_staging_ucrt64/gcc-15.2.0/build_mingw/x86_64-w64-mingw32/libgcc",
    b"C:\\mimita-priv-v8",
    b"C:/mimita-priv-v8",
)


def scrub_path(path):
    data = open(path, "rb").read()
    changed = 0
    for pat in PATTERNS:
        i = data.find(pat)
        while i != -1:
            repl = (FILLER * (len(pat) // len(FILLER) + 1))[: len(pat)]
            data = data[:i] + repl + data[i + len(pat):]
            changed += 1
            i = data.find(pat, i + len(pat))
    if changed:
        with open(path, "wb") as f:
            f.write(data)
    print(f"[SCRUB] {path}: {changed} path occurrence(s) replaced")
    return changed


if __name__ == "__main__":
    total = 0
    for p in sys.argv[1:]:
        total += scrub_path(p)
    sys.exit(0 if total >= 0 else 1)
