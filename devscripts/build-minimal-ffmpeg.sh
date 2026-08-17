#!/usr/bin/env bash
# 08 17 2026
# Builds the minimal static FFmpeg shipped as tools/ffmpeg.exe (single binary,
# no ffprobe). Run from an MSYS2 MINGW64 shell. Requirements:
#   pacman -S --needed base-devel mingw-w64-x86_64-gcc mingw-w64-x86_64-binutils \
#       mingw-w64-x86_64-gcc-libs mingw-w64-x86_64-nasm mingw-w64-x86_64-pkgconf git
# Output: <workspace>/ffmpeg/ffmpeg.exe  (copy it to tools/ffmpeg.exe)
#
# Pinned revisions (behavior must match the 2026-08-17 ship):
#   ffmpeg: n9.0  (d32b387)
#   x264:   0480cb0
#
# Feature whitelist (kept minimal on purpose). The game only uses:
#   main export : rawvideo -> libx264(+aac) mp4
#   outro       : single filter_complex concat (replay + webm outro)

set -euo pipefail

WS="${1:-$HOME/ffmpeg-build}"
FFMPEG_REF="${FFMPEG_REF:-n9.0}"
X264_REF="${X264_REF:-0480cb0}"

mkdir -p "$WS"
cd "$WS"

if [ ! -d x264 ]; then
    git clone --depth 1 https://code.videolan.org/videolan/x264.git x264
fi
cd x264
git fetch --depth 1 origin "$X264_REF" 2>/dev/null || true
git checkout "$X264_REF" 2>/dev/null || true
make distclean >/dev/null 2>&1 || true
./configure --enable-static --disable-shared --disable-cli \
    --bit-depth=8 --chroma-format=420
make -j"$(nproc)"
X264_DIR="$PWD"

cd "$WS"
if [ ! -d ffmpeg ]; then
    git clone --depth 1 --branch "$FFMPEG_REF" https://git.ffmpeg.org/ffmpeg.git ffmpeg
fi
cd ffmpeg
git fetch --depth 1 origin tag "$FFMPEG_REF" 2>/dev/null || true
git checkout "$FFMPEG_REF" 2>/dev/null || true
make distclean >/dev/null 2>&1 || true

# Write a correct x264.pc (x264's configure emits a /usr/local-prefixed one).
cat > "$X264_DIR/x264.pc" <<EOF
prefix=$X264_DIR
exec_prefix=\${prefix}
libdir=\${prefix}
includedir=\${prefix}

Name: x264
Description: H.264 (MPEG4 AVC) encoder library
Version: 0.165.x
Libs: -L\${prefix} -lx264
Libs.private:
Cflags: -I\${prefix}
EOF

export PKG_CONFIG_PATH="$X264_DIR"
./configure \
    --arch=x86_64 --target-os=mingw32 \
    --enable-gpl --enable-libx264 \
    --enable-static --disable-shared --enable-small \
    --disable-autodetect --disable-doc --disable-debug \
    --disable-avdevice --disable-network \
    --disable-everything --enable-ffmpeg \
    --enable-protocol=file \
    --enable-demuxer=rawvideo,wav,mov,matroska \
    --enable-muxer=mov,mp4 \
    --enable-decoder=rawvideo,h264,vp9,aac,opus,pcm_s16le,pcm_s16be,pcm_u8,pcm_s24le,pcm_f32le \
    --enable-encoder=libx264,aac \
    --enable-parser=h264,vp9,aac,opus \
    --enable-filter=scale,pad,setsar,fps,format,aformat,aresample,setpts,asetpts,concat \
    --extra-cflags="-I$X264_DIR" \
    --extra-ldflags="-L$X264_DIR -static" \
    --pkg-config-flags="--static"

make -j"$(nproc)"

echo
echo "Built: $PWD/ffmpeg.exe"
echo "Copy it into the repo:  cp ffmpeg.exe <repo>/tools/ffmpeg.exe"
ls -la ffmpeg.exe
