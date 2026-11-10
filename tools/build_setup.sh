#!/bin/sh
# Cross-build afrosetup-v<app/VERSION>.exe and append that source snapshot.
# The result is a Windows GUI program. It is not the game.
# make_setup_art.py copies the supplied Lomorsin icon. It does not draw one.
# This does not upload or replace a GitHub release asset.
set -eu
cd "$(dirname "$0")"
ROOT=$(CDPATH= cd .. && pwd)
REPO=${REPO:-/tmp/afro-0.2.1}
if [ ! -f "$REPO/install.bat" ]; then
  echo "REPO must be a full AfroSamuraiRecomp tree (install.bat missing: $REPO)" >&2
  exit 1
fi
VER=$(tr -d ' \t\r\n' < "$ROOT/app/VERSION")
case "$VER" in
  ''|*[!0-9.]*) echo "app/VERSION is not a dotted number: $VER" >&2; exit 1 ;;
esac
OUT="afrosetup-v${VER}.exe"
CXX=${CXX:-x86_64-w64-mingw32-g++}
WINDRES=${WINDRES:-x86_64-w64-mingw32-windres}
python3 make_setup_art.py
"$WINDRES" -O coff afrosetup.rc -o afrosetup.res
"$CXX" -std=c++17 -O2 -Wall -Wextra -Werror -DUNICODE -D_UNICODE -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 \
  -municode -mwindows -static -static-libgcc -static-libstdc++ \
  -o "$OUT" afrosetup.cpp afrosetup.res \
  -lcomdlg32 -lshell32 -lole32 -luuid -lgdi32 -lcomctl32
python3 pack_setup_payload.py --exe "$OUT" --repo "$REPO" --overlay "$ROOT"
echo "built $(pwd)/$OUT"
