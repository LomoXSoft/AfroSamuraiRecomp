#!/bin/sh
# Cross-build afrosetup.exe and append the 0.1.0 source snapshot.
# The result is a Windows GUI program. It is not the game.
set -eu
cd "$(dirname "$0")"
ROOT=$(CDPATH= cd .. && pwd)
REPO=${REPO:-/agent/repos/AfroSamuraiRecomp}
if [ ! -f "$REPO/install.bat" ]; then
  echo "REPO must be a full AfroSamuraiRecomp tree (install.bat missing: $REPO)" >&2
  exit 1
fi
CXX=${CXX:-x86_64-w64-mingw32-g++}
WINDRES=${WINDRES:-x86_64-w64-mingw32-windres}
python3 make_setup_icon.py
"$WINDRES" -O coff afrosetup.rc -o afrosetup.res
"$CXX" -std=c++17 -O2 -Wall -Wextra -Werror -DUNICODE -D_UNICODE -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 \
  -municode -mwindows -static -static-libgcc -static-libstdc++ \
  -o afrosetup.exe afrosetup.cpp afrosetup.res \
  -lcomdlg32 -lshell32 -lole32 -luuid -lgdi32 -lcomctl32
python3 pack_setup_payload.py --exe afrosetup.exe --repo "$REPO" --overlay "$ROOT"
echo "built $(pwd)/afrosetup.exe"
