#!/usr/bin/env bash
# AfroSamuraiRecomp installer for Linux and SteamOS (Steam Deck, Steam Machine).
#
# Builds the WINDOWS game from your own Afro Samurai disc (Europe or USA) and
# sets it up to run through Proton. It is not a native Linux port.
#
# No root, no package manager: every tool is downloaded from its official
# source (pinned, SHA-256 checked) into <install folder>/tools. Needs only
# bash, python3 (3.9 or newer), git, curl, tar and xz, which SteamOS has.
#
# Safe to re-run: finished steps are remembered in <install folder>/.install.
# Nothing is uploaded. Log: <install folder>/install.log
#
# usage: ./install.sh [options]
#   --dir <folder>          install folder (default: ~/Games/AfroSamuraiRecomp)
#   --iso <file>            your Afro Samurai ISO (Europe or USA)
#   --gamedata <folder>     an already extracted disc (must contain default.xex)
#   --update                rebuild an existing installation from this source;
#                           keeps gamedata, app/region, saves and settings
#   --accept-msvc-license   accept the Microsoft license for the MSVC runtime
#                           and Windows SDK headers without asking
#   --language en|es|fr|it  game language for a new install (default: en)
#   --jobs <n>              parallel compile jobs (default: all CPU threads)
#   --cleanup               delete the build files at the end (no question)
#   --keep-build-files      keep the build files at the end (no question)
#   --yes                   do not ask; fail if an answer is missing
set -Eeuo pipefail
umask 022

# ------------------------------------------------------------ pins --------
REXGLUE_COMMIT=bd833a2
REXGLUE_GIT=https://github.com/rexglue/rexglue-sdk.git
NIGHTLY=https://github.com/rexglue/rexglue-sdk/releases/download/nightly-20261002-bd833a2a
# ReXGlue keeps only its last ten nightlies, so this one is gone there; the
# project keeps the same files (BSD-3-Clause) on its own release.
SDK_MIRROR=https://github.com/LomoXSoft/AfroSamuraiRecomp/releases/download/rexglue-sdk-bd833a2
XWIN_CRT=14.44.17.14    # MSVC 14.52 and later refuse Clang 20 (see tools/install.ps1)
XWIN_SDK=10.0.26100
ISO_SHA1=29394db1854dfc59c767d768c8d35e0a08acb73d   # Europe ISO; USA is checked by default.xex
XEX_EUROPE=5e831c64173b4a24805bc9c0c0eb860b3d7f53c6
XEX_USA=873fff7191be2b687eb55f8564f9b9f5c5842b40
MSVC_LICENSE=https://go.microsoft.com/fwlink/?LinkId=2327714
NEED_GB=20
# name|url|sha256|file[|mirror url, tried first]
DOWNLOADS=(
  "llvm|https://github.com/mstorsjo/llvm-mingw/releases/download/20250709/llvm-mingw-20250709-ucrt-ubuntu-22.04-x86_64.tar.xz|60cafae6474c7411174cff1d4ba21a8e46cadbaeb05a1bace306add301628337|llvm-mingw-20250709-ucrt-ubuntu-22.04-x86_64.tar.xz"
  "cmake|https://github.com/Kitware/CMake/releases/download/v4.4.5/cmake-4.4.5-linux-x86_64.tar.gz|ed85135c5a5135b45e50423cb406dea04ca3338da0b8ef3c9ede963559680be0|cmake-4.4.5-linux-x86_64.tar.gz"
  "ninja|https://github.com/ninja-build/ninja/releases/download/v1.13.2/ninja-linux.zip|5749cbc4e668273514150a80e387a957f933c6ed3f5f11e03fb30955e2bbead6|ninja-linux-1.13.2.zip"
  "xwin|https://github.com/Jake-Shadle/xwin/releases/download/0.10.0/xwin-0.10.0-x86_64-unknown-linux-musl.tar.gz|d870eb4b2f390878af6da1ccd3cf321d22fcb72720984853b4be732ae597fc88|xwin-0.10.0-x86_64-unknown-linux-musl.tar.gz"
  "pycryptodome|https://files.pythonhosted.org/packages/af/1b/b0f5f020bd43351932df9d16857330d3304715c4b1f7f783dc6e58cb7919/pycryptodome-4.3.0-cp39-abi3-manylinux2014_x86_64.manylinux_2_17_x86_64.whl|96bf142ef25544a221a2b4a8f5d449cb07c1736be71b4ea057749318a2cd402c|pycryptodome-4.3.0-cp39-abi3-manylinux2014_x86_64.whl"
  "sdk-linux|$NIGHTLY/rexglue-sdk-0.10.0.24-dev.gbd833a2-linux-amd64.zip|0a56565221a4e75f91fc71f787d9299a0d0298127b970c8a88a292f8d3a3befe|rexglue-sdk-0.10.0.24-dev.gbd833a2-linux-amd64.zip|$SDK_MIRROR/rexglue-sdk-0.10.0.24-dev.gbd833a2-linux-amd64.zip"
  "sdk-win|$NIGHTLY/rexglue-sdk-0.10.0.24-dev.gbd833a2-win-amd64.zip|1e19d7ec9be0d9d7e1a0088a34a33b068d4dea520181e81318528ef3b37627e7|rexglue-sdk-0.10.0.24-dev.gbd833a2-win-amd64.zip|$SDK_MIRROR/rexglue-sdk-0.10.0.24-dev.gbd833a2-win-amd64.zip"
)
# Changing any of these makes the next run compile from scratch.
TOOL_KEY="llvm-mingw-20250709 cmake-4.4.5 ninja-1.13.2 xwin-$XWIN_CRT-$XWIN_SDK rexglue-$REXGLUE_COMMIT"

# ------------------------------------------------------------ options -----
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT=''; ISO=''; GAMEDATA_DIR=''; UPDATE=0; ACCEPT_LICENSE=0; LANGUAGE=''
JOBS=''; CLEANUP=''; YES=0
die() { echo "ERROR: $*" >&2; exit 1; }
while [ $# -gt 0 ]; do
  case "$1" in
    --dir|--root) ROOT=${2:?--dir needs a folder}; shift ;;
    --iso) ISO=${2:?--iso needs a file}; shift ;;
    --gamedata) GAMEDATA_DIR=${2:?--gamedata needs a folder}; shift ;;
    --update) UPDATE=1 ;;
    --accept-msvc-license) ACCEPT_LICENSE=1 ;;
    --language) LANGUAGE=${2:?--language needs en, es, fr or it}; shift ;;
    --jobs) JOBS=${2:?--jobs needs a number}; shift ;;
    --cleanup) CLEANUP=yes ;;
    --keep-build-files) CLEANUP=no ;;
    --yes|--non-interactive) YES=1 ;;
    -h|--help) sed -n '2,27p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) die "unknown option: $1 (see ./install.sh --help)" ;;
  esac
  shift
done

TTY=/dev/tty
can_ask() { [ "$YES" = 0 ] && { true < "$TTY"; } 2>/dev/null; }
ask() {  # ask <question> <default> -> answer on stdout
  local a=''
  if can_ask; then printf '%s [%s] ' "$1" "$2" > "$TTY"; IFS= read -r a < "$TTY" || true; fi
  a=${a#"${a%%[![:space:]]*}"}; a=${a%"${a##*[![:space:]]}"}
  a=${a#\"}; a=${a%\"}; a=${a#\'}; a=${a%\'}
  printf '%s' "${a:-$2}"
}
yesno() {  # yesno <question> <y|n>
  local a; a=$(ask "$1 (y/n)" "$2")
  case "$a" in [yYsS]*) return 0 ;; *) return 1 ;; esac
}
abspath() { python3 -c 'import os,sys; print(os.path.abspath(os.path.expanduser(sys.argv[1])))' "$1"; }

# The default folder: next to the source when this script runs from an
# installed copy (<root>/afrorecomp/install.sh), else ~/Games/AfroSamuraiRecomp.
if [ -z "$ROOT" ]; then
  if [ "$(basename "$HERE")" = afrorecomp ] && [ -d "$HERE/../.install" ]; then
    ROOT=$(cd "$HERE/.." && pwd)
  elif [ "$UPDATE" = 1 ] || [ "$YES" = 1 ]; then
    ROOT=$HOME/Games/AfroSamuraiRecomp
  else
    ROOT=$(ask 'Install folder' "$HOME/Games/AfroSamuraiRecomp")
  fi
fi
ROOT=$(abspath "$ROOT")
case "$ROOT" in /|"$HOME"|/home|/usr|/usr/*|/etc|/etc/*) die "refusing to install into $ROOT" ;; esac
REPO=$ROOT/afrorecomp
TOOLS=$ROOT/tools
DL=$ROOT/downloads
STAMPS=$ROOT/.install
SDKSRC=$ROOT/rexglue-sdk
GAME=$ROOT/game
GAMEDATA=$ROOT/gamedata
mkdir -p "$ROOT" "$STAMPS" "$DL" "$TOOLS"
LOG=$ROOT/install.log
exec > >(tee -a "$LOG") 2>&1
STEP=start
trap 'echo; echo "FAILED at: $STEP"; echo "Full log: $LOG"; echo "Fix the problem and run the same command again: finished steps are skipped."' ERR

say() { echo "$*"; }
step() { STEP=$1; echo; echo "==> $1"; }
done_() { [ -e "$STAMPS/$1.done" ]; }
mark() { : > "$STAMPS/$1.done"; }
T0=$(date +%s)
echo
echo "=== AfroSamuraiRecomp Linux installer, $(date '+%Y-%m-%d %H:%M:%S') ==="
say "install folder: $ROOT"
say "source: $HERE ($(tr -d ' \r\n' < "$HERE/app/VERSION" 2>/dev/null || echo 'no app/VERSION'))"

# ------------------------------------------------------------ checks ------
step 'system checks'
[ "$(uname -m)" = x86_64 ] || die "needs an x86_64 PC (this is $(uname -m))"
[ -f "$HERE/app/VERSION" ] && [ -f "$HERE/tools/linux/win-cross.cmake" ] || die "run install.sh from a complete AfroSamuraiRecomp source folder"
for c in python3 git curl tar xz sha256sum sha1sum df; do
  command -v "$c" >/dev/null 2>&1 || die "'$c' is missing. SteamOS and most desktop Linux systems have it."
done
python3 -c 'import sys; sys.exit(sys.version_info < (3, 9))' || die "python3 3.9 or newer is needed (found $(python3 --version 2>&1))"
case "${LANGUAGE:-en}" in en|es|fr|it) ;; *) die "--language must be en, es, fr or it" ;; esac
[ -n "$JOBS" ] || JOBS=$(nproc 2>/dev/null || echo 4)
case "$JOBS" in ''|*[!0-9]*|0) die "--jobs must be a positive number" ;; esac
afro_running() { pgrep -f 'afrorecomp\.exe' >/dev/null 2>&1; }
if [ "$UPDATE" = 1 ]; then
  [ -f "$GAME/afrorecomp.exe" ] || die "$ROOT is not an AfroSamuraiRecomp installation (game/afrorecomp.exe is missing)"
  afro_running && die "afrorecomp.exe is running. Close the game first; this installer will not stop it."
  say "update $(tr -d ' \r\n' < "$REPO/app/VERSION" 2>/dev/null || echo 'unknown') -> $(tr -d ' \r\n' < "$HERE/app/VERSION")"
  # Program files are rebuilt and swapped in only after the build succeeds.
  rm -f "$STAMPS/build.done"
fi
free_gb=$(df -Pk "$ROOT" | awk 'NR==2 {printf "%d", $4/1048576}')
say "free space in $ROOT: ${free_gb} GB"
if ! done_ build && [ "$free_gb" -lt "$NEED_GB" ]; then die "about $NEED_GB GB of free space is needed (found $free_gb GB)"; fi
say "parallel jobs: $JOBS, memory: $(awk '/MemTotal/ {printf "%d GB", $2/1048576}' /proc/meminfo)"

# ------------------------------------------------------------ questions ---
# Every question is asked here, before any download, so the rest runs unattended.
step 'questions'
if [ ! -f "$GAMEDATA/default.xex" ] && [ -z "$GAMEDATA_DIR" ] && [ -z "$ISO" ]; then
  can_ask || die "no game files: pass --iso <file> or --gamedata <folder>"
  ISO=$(ask 'Path to your Afro Samurai ISO (Europe or USA)' '')
  [ -n "$ISO" ] || die "no ISO given"
fi
[ -z "$ISO" ] || { ISO=$(abspath "$ISO"); [ -f "$ISO" ] || die "ISO not found: $ISO"; }
[ -z "$GAMEDATA_DIR" ] || { GAMEDATA_DIR=$(abspath "$GAMEDATA_DIR"); [ -f "$GAMEDATA_DIR/default.xex" ] || die "no default.xex in $GAMEDATA_DIR"; }
if ! done_ xwin-splat && [ "$ACCEPT_LICENSE" = 0 ]; then
  echo
  echo "The game is compiled against the Microsoft C++ runtime headers and libraries"
  echo "and the Windows SDK (downloaded from Microsoft by xwin). They come under the"
  echo "Microsoft Visual Studio Build Tools license:"
  echo "  $MSVC_LICENSE"
  can_ask || die "pass --accept-msvc-license to accept the Microsoft license without a question"
  yesno 'Do you accept that license?' n || die "the Microsoft license was not accepted; nothing to build without it"
  say "Microsoft license accepted by the user"
fi
if [ "$UPDATE" = 0 ] && [ -z "$LANGUAGE" ]; then
  LANGUAGE=$(ask 'Game language: en, es, fr or it' en)
  case "$LANGUAGE" in en|es|fr|it) ;; *) die "language must be en, es, fr or it" ;; esac
fi

# ------------------------------------------------------------ downloads ---
fetch() {  # fetch <name> -> path of the verified file on stdout
  local entry name url sha file mirror out got src
  for entry in "${DOWNLOADS[@]}"; do
    IFS='|' read -r name url sha file mirror <<< "$entry"
    [ "$name" = "$1" ] || continue
    out=$DL/$file
    if [ -f "$out" ] && [ "$(sha256sum "$out" | cut -d' ' -f1)" = "$sha" ]; then echo "$out"; return; fi
    got=''
    for src in $mirror $url; do
      echo "    downloading $file ($src)" >&2
      if curl -fsSL --retry 3 --retry-delay 2 -C - -o "$out.part" "$src" >&2 \
         || { rm -f "$out.part"; curl -fsSL --retry 3 -o "$out.part" "$src" >&2; }; then
        got=$(sha256sum "$out.part" | cut -d' ' -f1)
        [ "$got" = "$sha" ] && break
        echo "    SHA-256 mismatch for $file from $src (got $got, expected $sha)" >&2
      else
        echo "    download failed from $src" >&2
      fi
      rm -f "$out.part"; got=''
    done
    [ -n "$got" ] || die "could not download $file (SHA-256 $sha)"
    mv -f "$out.part" "$out"
    echo "    ok, $(( $(stat -c %s "$out") / 1048576 )) MB, SHA-256 verified" >&2
    echo "$out"; return
  done
  die "unknown download $1"
}
unzip_to() { python3 -m zipfile -e "$1" "$2"; }
untar_to() { rm -rf "$2"; mkdir -p "$2"; tar -xf "$1" -C "$2" --strip-components=1; }

step 'toolchain'
if ! done_ llvm || [ ! -x "$TOOLS/llvm-mingw/bin/clang" ]; then
  say "  LLVM/Clang 20.1.8 (llvm-mingw, about 76 MB)"; f=$(fetch llvm); untar_to "$f" "$TOOLS/llvm-mingw"; mark llvm
fi
if ! done_ cmake || [ ! -x "$TOOLS/cmake/bin/cmake" ]; then
  say "  CMake 4.4.5"; f=$(fetch cmake); untar_to "$f" "$TOOLS/cmake"; mark cmake
fi
if ! done_ ninja || [ ! -x "$TOOLS/ninja/ninja" ]; then
  say "  Ninja 1.13.2"; f=$(fetch ninja); rm -rf "$TOOLS/ninja"; unzip_to "$f" "$TOOLS/ninja"; chmod +x "$TOOLS/ninja/ninja"; mark ninja
fi
if ! done_ xwin || [ ! -x "$TOOLS/xwin/xwin" ]; then
  say "  xwin 0.10.0"; f=$(fetch xwin); untar_to "$f" "$TOOLS/xwin"; mark xwin
fi
if ! done_ pycryptodome || [ ! -d "$TOOLS/pylib/Crypto" ]; then
  say "  pycryptodome 4.3.0"; f=$(fetch pycryptodome); rm -rf "$TOOLS/pylib"; unzip_to "$f" "$TOOLS/pylib"; mark pycryptodome
fi
export PATH="$TOOLS/llvm-mingw/bin:$TOOLS/cmake/bin:$TOOLS/ninja:$PATH"
export PYTHONPATH="$TOOLS/pylib${PYTHONPATH:+:$PYTHONPATH}"
python3 -c 'from Crypto.Cipher import AES' || die "pycryptodome does not load with $(python3 --version 2>&1)"
clang --version | head -1

step 'ReXGlue SDK nightly 0.10.0.24-dev.gbd833a2'
if ! done_ sdk || [ ! -f "$ROOT/sdk/linux-amd64/bin/rexglue" ] || [ ! -d "$ROOT/sdk/win-amd64/lib/cmake/rexglue" ]; then
  f=$(fetch sdk-linux); g=$(fetch sdk-win)
  rm -rf "$ROOT/sdk"; mkdir -p "$ROOT/sdk"
  unzip_to "$f" "$ROOT/sdk"
  unzip_to "$g" "$ROOT/sdk"
  mark sdk
fi
REXGLUE=$ROOT/sdk/linux-amd64/bin/rexglue
chmod +x "$REXGLUE"
"$REXGLUE" --version >/dev/null 2>&1 || {
  "$REXGLUE" --version || true
  die "the Linux rexglue does not start. It needs glibc 2.34 and libstdc++ from GCC 13.2 or newer (GLIBCXX_3.4.32)."
}
say "    rexglue $("$REXGLUE" --version 2>&1 | head -1)"
# The Windows package names rexglue.exe for the codegen rule of the build: point it
# at the Linux tool (every run, so a moved install folder still works).
python3 - "$ROOT/sdk/win-amd64/lib/cmake/rexglue" "$REXGLUE" <<'PY'
import glob, os, re, sys
d, tool = sys.argv[1], sys.argv[2]
for p in glob.glob(os.path.join(d, 'rexglueTargets-*.cmake')):
    s = open(p, encoding='utf-8').read()
    t = re.sub(r'"[^"\n]*/bin/rexglue(\.exe)?"', '"%s"' % tool, s)
    if t != s:
        open(p, 'w', encoding='utf-8').write(t)
PY

step 'Microsoft C++ runtime and Windows SDK (xwin)'
XWIN=$TOOLS/xwin-sdk
if ! done_ xwin-splat || [ ! -d "$XWIN/crt/include" ]; then
  rm -rf "$XWIN"
  "$TOOLS/xwin/xwin" --accept-license --cache-dir "$DL/xwin-cache" --manifest-version 17 \
    --crt-version "$XWIN_CRT" --sdk-version "$XWIN_SDK" --arch x86_64 --variant desktop \
    splat --output "$XWIN"
  rm -rf "$DL/xwin-cache"
  mark xwin-splat
fi
export INCLUDE="$XWIN/crt/include;$XWIN/sdk/include/ucrt;$XWIN/sdk/include/um;$XWIN/sdk/include/shared;$XWIN/sdk/include/winrt"
export LIB="$XWIN/crt/lib/x86_64;$XWIN/sdk/lib/um/x86_64;$XWIN/sdk/lib/ucrt/x86_64"

# ------------------------------------------------------------ sources -----
step 'game source'
python3 "$HERE/tools/linux/sync_tree.py" "$HERE" "$REPO" \
  app/out app/generated/default app/assets app/afro_switch_tables.toml app/afro_icon.ico app/region
VER=$(tr -d ' \r\n' < "$REPO/app/VERSION")
REV="local-$VER"
if git -C "$HERE" rev-parse HEAD >/dev/null 2>&1; then REV="local-$VER-$(git -C "$HERE" rev-parse HEAD)"; fi
say "    version $VER ($REV)"
if [ "$(cat "$STAMPS/build.rev" 2>/dev/null || true)" != "$REV" ]; then rm -f "$STAMPS/build-app.done" "$STAMPS/build-gpu.done" "$STAMPS/build.done"; fi

step "ReXGlue source at $REXGLUE_COMMIT"
if ! done_ rexglue-src || [ ! -d "$SDKSRC/.git" ]; then
  if [ ! -d "$SDKSRC/.git" ]; then
    rm -rf "$SDKSRC"
    git clone --filter=blob:none --no-checkout "$REXGLUE_GIT" "$SDKSRC"
  fi
  git -C "$SDKSRC" -c advice.detachedHead=false checkout -f "$REXGLUE_COMMIT"
  git -C "$SDKSRC" submodule update --init --recursive --depth 1 --jobs 8 ||
    { say "    shallow submodules failed, fetching them in full"; git -C "$SDKSRC" submodule update --init --recursive --jobs 8; }
  mark rexglue-src
fi

step 'Afro patches for ReXGlue'
# Same rules as tools/build_gpu_plugin.bat: a stamp per patch (name + blob
# hash) in .git/afro-patches; if the whole stack already reverse-applies, all
# patches are stamped as applied.
QUAD=zzz-afro-quad-depth-offset.patch
QUAD_OLD='PipelineCache::PipelineGeometryShader geometry_shader'
mapfile -t PATCHES < <(printf '%s\n' "$REPO/patches/rexglue/"*.patch | LC_ALL=C sort)
[ -f "${PATCHES[0]}" ] || die "no patches in $REPO/patches/rexglue"
(
  trap - ERR   # the main script reports the failure once
  cd "$SDKSRC"
  python3 "$REPO/tools/repair_quad_depth_access.py" "$PWD"
  mkdir -p .git/afro-patches
  idx=$(mktemp)
  stack_only=1
  GIT_INDEX_FILE=$idx git read-tree HEAD
  GIT_INDEX_FILE=$idx git add -u >/dev/null 2>&1 || true
  GIT_INDEX_FILE=$idx git add -A -- src include >/dev/null 2>&1 || true
  for (( i=${#PATCHES[@]}-1; i>=0; i-- )); do
    GIT_INDEX_FILE=$idx git apply --ignore-whitespace --reverse --cached "${PATCHES[$i]}" >/dev/null 2>&1 || stack_only=0
  done
  rm -f "$idx"
  if [ "$stack_only" = 1 ]; then echo "    all patches already applied (stack check)"; fi
  for p in "${PATCHES[@]}"; do
    n=$(basename "$p"); st=.git/afro-patches/$n.$(git hash-object "$p")
    if [ -e "$st" ]; then
      if [ "$n" != "$QUAD" ] || ! grep -qF "$QUAD_OLD" src/graphics/d3d12/pipeline_cache.cpp; then echo "    $n already applied [stamp]"; continue; fi
      echo "    $n stamp kept a helper that cannot see the private enum"
    fi
    if [ "$stack_only" = 1 ]; then
      if [ "$n" = "$QUAD" ] && grep -qF "$QUAD_OLD" src/graphics/d3d12/pipeline_cache.cpp; then
        echo "    $n stack stamp would hide the private-enum helper"; exit 1
      fi
      : > "$st"; echo "    $n already applied [stack]"; continue
    fi
    if git apply --ignore-whitespace --reverse --check "$p" >/dev/null 2>&1; then : > "$st"; echo "    $n already applied"; continue; fi
    if ! git apply --ignore-whitespace "$p"; then
      [ "$n" = "$QUAD" ] || { echo "PATCH FAILED: $n (if it is already in the tree, create $SDKSRC/$st)"; exit 1; }
      grep -qF 'bool PipelineCache::AfroQuadDepthShiftEnabled' src/graphics/d3d12/pipeline_cache.cpp || { echo "PATCH FAILED: $n"; exit 1; }
      grep -qF 'static bool AfroQuadDepthShiftEnabled' include/rex/graphics/d3d12/pipeline_cache.h || { echo "PATCH FAILED: $n missing the private helper declaration"; exit 1; }
      echo "    $n already contained the repaired helper"
    fi
    : > "$st"; echo "    $n applied"
  done
)
# Windows names are case-insensitive: add the spellings the sources use.
python3 "$REPO/tools/linux/xwin_case_links.py" "$XWIN" "$SDKSRC" "$REPO/app"

# ------------------------------------------------------------ game data ---
step 'game files'
if [ ! -f "$GAMEDATA/default.xex" ]; then
  if [ -n "$GAMEDATA_DIR" ]; then
    say "    copying $GAMEDATA_DIR"
    rm -rf "$GAMEDATA.tmp"; cp -a "$GAMEDATA_DIR" "$GAMEDATA.tmp"; mv "$GAMEDATA.tmp" "$GAMEDATA"
  else
    say "    checking the ISO (SHA-1, takes a minute)"
    iso_sha=$(sha1sum "$ISO" | cut -d' ' -f1)
    if [ "$iso_sha" = "$ISO_SHA1" ]; then say "    Europe ISO verified"
    else say "    ISO SHA-1 is $iso_sha, not the known Europe ISO. Not verified; continuing. default.xex is checked after extraction (Europe or USA)."; fi
    say "    extracting the disc"
    rm -rf "$GAMEDATA.tmp"
    python3 "$REPO/tools/xiso_extract.py" "$ISO" "$GAMEDATA.tmp" /
    mv "$GAMEDATA.tmp" "$GAMEDATA"
  fi
fi
[ -f "$GAMEDATA/default.xex" ] || die "gamedata/default.xex is missing after extraction"
xex_sha=$(sha1sum "$GAMEDATA/default.xex" | cut -d' ' -f1)
case "$xex_sha" in
  "$XEX_EUROPE") say "    default.xex: Europe" ;;
  "$XEX_USA") say "    default.xex: USA" ;;
  *) die "default.xex (SHA-1 $xex_sha) is not from the Europe or USA disc, or the extraction is incomplete" ;;
esac

# ------------------------------------------------------------ build -------
OUT=$REPO/app/out/build/win-amd64-release
GPUOUT=$SDKSRC/out/win-amd64/Release
if [ "$(cat "$STAMPS/build-tools.key" 2>/dev/null || true)" != "$TOOL_KEY" ]; then
  if [ -d "$REPO/app/out" ] || [ -d "$SDKSRC/out" ]; then say "    toolchain changed: build folders cleared, full compile"; fi
  rm -rf "$REPO/app/out" "$SDKSRC/out/build/win-amd64-cross" "$SDKSRC/out/win-amd64"
  rm -f "$STAMPS/build-app.done" "$STAMPS/build-gpu.done" "$STAMPS/build.done"
fi
echo "$TOOL_KEY" > "$STAMPS/build-tools.key"
TOOLCHAIN=$REPO/tools/linux/win-cross.cmake

step 'build the game (codegen + compile, the longest step)'
(
  trap - ERR   # the main script reports the failure once
  cd "$REPO/app"
  mkdir -p assets
  cmp -s "$GAMEDATA/default.xex" assets/default.xex || cp -f "$GAMEDATA/default.xex" assets/default.xex
  python3 ../tools/region_config.py .
  if [ ! -f afro_switch_tables.toml ]; then
    python3 ../tools/xex_to_pe.py assets/default.xex assets/default.pe "$SDKSRC"
    python3 ../tools/gen_switch_tables.py assets/default.pe afro_switch_tables.toml
    python3 ../tools/extract_icon.py assets/default.pe afro_icon.ico || true
  fi
  configure() {
    cmake -S . -B "$OUT" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
      -DCMAKE_PREFIX_PATH="$ROOT/sdk/win-amd64" > "$ROOT/cmake_app.txt" 2>&1 || { tail -40 "$ROOT/cmake_app.txt"; exit 1; }
  }
  s=$(date +%s)
  configure
  # The recompiled sources are listed by generated/default/sources.cmake, which
  # codegen writes: without it CMake configures a build without them. Run the
  # codegen target first and configure again (as tools/build_win.bat does by
  # running codegen before CMake). Ninja starts codegen once more after the new
  # configure; rexglue then finds its inputs unchanged and writes nothing.
  if [ ! -f generated/default/sources.cmake ]; then
    echo "    codegen (C++ from default.xex, a few minutes)"
    ninja -C "$OUT" afrorecomp_codegen
    [ -f generated/default/sources.cmake ] || { echo "codegen wrote no generated/default/sources.cmake"; exit 1; }
    configure
  fi
  ninja -C "$OUT" -j "$JOBS"
  echo "    game build: $(( $(date +%s) - s )) s"
)
mark build-app

step 'build the patched GPU plugin and runtime'
(
  trap - ERR   # the main script reports the failure once
  cd "$SDKSRC"
  cmake -S . -B out/build/win-amd64-cross -G 'Ninja Multi-Config' -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DCMAKE_C_FLAGS=-march=x86-64-v2 -DCMAKE_CXX_FLAGS=-march=x86-64-v2 -DCMAKE_CXX_STANDARD=23 \
    '-DCMAKE_CONFIGURATION_TYPES=Debug;Release;RelWithDebInfo' \
    -DREXGLUE_BUILD_TESTS=OFF -DREXGLUE_ENABLE_TRACY=OFF > "$ROOT/cmake_gpu.txt" 2>&1 || { tail -40 "$ROOT/cmake_gpu.txt"; exit 1; }
  s=$(date +%s)
  cmake --build out/build/win-amd64-cross --config Release --target rexgpu-xenos rexruntime -j "$JOBS"
  echo "    GPU plugin + runtime build: $(( $(date +%s) - s )) s"
)
mark build-gpu

step 'verify the build'
[ -f "$OUT/afrorecomp.exe" ] || die "afrorecomp.exe is missing in $OUT"
grep -qa afro_xma_frameless_chain "$GPUOUT/rexruntime.dll" || die "rexruntime.dll is not the patched one"
grep -qa afro_edram_no_wrap "$GPUOUT/rexgpu-xenos.dll" || die "rexgpu-xenos.dll is not the patched one"
say "    afrorecomp.exe, rexruntime.dll and rexgpu-xenos.dll ok"
mark build; echo "$REV" > "$STAMPS/build.rev"

# ------------------------------------------------------------ game folder -
step 'game folder'
# The app build copies the stock SDK DLLs next to the exe: the patched ones replace them.
STAGE=$GAME/.update-staging
rm -rf "$STAGE"; mkdir -p "$STAGE"
for f in "$OUT"/*; do
  [ -f "$f" ] || continue
  case "$(basename "$f")" in
    *.stock|*.pdb|*.ilk|*.lib|*.exp|*.ninja|*.cmake|*.log|CMakeCache.txt|.ninja_*|compile_commands.json) continue ;;
  esac
  cp -f "$f" "$STAGE/"
done
cp -f "$GPUOUT/rexruntime.dll" "$GPUOUT/rexgpu-xenos.dll" "$STAGE/"
for n in afrorecomp.exe rexruntime.dll rexgpu-xenos.dll; do
  [ -f "$STAGE/$n" ] || { rm -rf "$STAGE"; die "new build is missing $n; the installed game folder was not changed"; }
done
if [ "$UPDATE" = 1 ]; then afro_running && { rm -rf "$STAGE"; die "afrorecomp.exe is running; the game folder was not changed"; }; fi
# Swap: the previous files stay until every new file is staged; a failed move puts them back.
PREV=$GAME/.update-prev
rm -rf "$PREV"; mkdir -p "$PREV"
moved=()
swap_ok=1
for f in "$STAGE"/*; do
  n=$(basename "$f")
  if [ -e "$GAME/$n" ]; then mv -f "$GAME/$n" "$PREV/$n" || { swap_ok=0; break; }; moved+=("$n"); fi
  mv -f "$f" "$GAME/$n" || { swap_ok=0; break; }
done
if [ "$swap_ok" = 0 ]; then
  for n in "${moved[@]}"; do [ -e "$PREV/$n" ] && mv -f "$PREV/$n" "$GAME/$n"; done
  die "replacing the program files failed; the previous files were put back"
fi
rm -rf "$STAGE" "$PREV"
[ -f "$REPO/app/afro_icon.ico" ] && cp -f "$REPO/app/afro_icon.ico" "$GAME/afro_icon.ico"
if [ -d "$REPO/tools/artwork/glyphs" ]; then
  for fam in "$REPO/tools/artwork/glyphs"/*/; do
    fam=${fam%/}; mkdir -p "$GAME/glyphs/$(basename "$fam")"
    cp -f "$fam"/* "$GAME/glyphs/$(basename "$fam")/"
  done
  say "    button icons copied to game/glyphs"
fi
cp -f "$REPO/tools/recomplauncher.json" "$GAME/recomplauncher.json"
cp -f "$REPO/tools/recomplauncher.json" "$ROOT/recomplauncher.json"
# Settings: the launcher window has no controller support, so under Proton it is
# skipped (ask=0 and this version). Settings are in the game: F1, or hold both
# stick clicks. A new install writes the chosen language; an update changes only
# the version line and keeps every setting.
CFG=$GAME/afro_language.cfg
python3 - "$CFG" "$VER" "$UPDATE" "${LANGUAGE:-en}" <<'PY'
import os, sys
path, ver, update, lang = sys.argv[1], sys.argv[2], sys.argv[3] == '1', sys.argv[4]
xid = {'en': 1, 'es': 5, 'fr': 4, 'it': 6}[lang]
lines = []
if os.path.exists(path):
    lines = [l.rstrip('\r\n') for l in open(path, encoding='latin-1')]
drop = ('version=',) if update else ('version=', 'language=', 'ask=')
lines = [l for l in lines if l and not l.startswith(drop)]
if not update:
    lines = ['language=%d' % xid, 'ask=0'] + lines
lines.append('version=' + ver)
open(path, 'w', encoding='latin-1', newline='\r\n').write('\n'.join(lines) + '\n')
PY
say "    $( [ "$UPDATE" = 1 ] && echo 'settings kept (version line updated)' || echo "language set: $LANGUAGE" )"

STEAM_TXT=$ROOT/STEAM_SETUP.txt
cat > "$STEAM_TXT" <<EOF
Afro Samurai Recomp under Proton (Steam)

1. In Desktop Mode, open Steam: Games > Add a Non-Steam Game to My Library...
   > Browse..., set the file type to "All Files", and choose
     $GAME/afrorecomp.exe
   then Add Selected Programs.
2. Right-click the new entry > Properties > Shortcut:
     Name:           Afro Samurai Recomp
     Start in:       "$GAME"
     Launch options: --game_data_root="../gamedata" --gpu_plugin=xenos --log_file="../afro.log"
3. Properties > Compatibility: tick "Force the use of a specific Steam Play
   compatibility tool" and choose Proton (the newest stable version).
4. Play. In game, press F1 or hold both stick clicks for the options.

Log: $ROOT/afro.log
Saves are in the Proton prefix of the shortcut:
  ~/.local/share/Steam/steamapps/compatdata/<shortcut id>/pfx/drive_c/users/steamuser/Documents/afrorecomp
To update: get the new source, then run its install.sh --update --dir "$ROOT"
EOF

mins=$(( ($(date +%s) - T0 + 59) / 60 ))
echo
echo "=== Done in about $mins min. ==="
echo "Game folder: $GAME"
cat "$STEAM_TXT"
echo "(These steps are also in $STEAM_TXT)"

# ------------------------------------------------------------ cleanup -----
STEP=cleanup
JUNK=()
for d in tools downloads rexglue-sdk sdk afrorecomp .install; do
  p=$ROOT/$d
  [ -e "$p" ] || continue
  [ "$p" = "$HERE" ] && continue                        # never the source this runs from
  case "$HERE/" in "$p"/*) continue ;; esac
  [ -n "$ISO" ] && case "$ISO" in "$p"/*) continue ;; esac
  JUNK+=("$p")
done
if [ "${#JUNK[@]}" -gt 0 ] && [ "$CLEANUP" != no ]; then
  gb=$(du -sck "${JUNK[@]}" 2>/dev/null | tail -1 | awk '{printf "%.1f", $1/1048576}')
  if [ -z "$CLEANUP" ]; then
    echo
    echo "The build files take $gb GB. The game needs only $GAME and $GAMEDATA."
    echo "Keeping them makes the next update much faster."
    if can_ask && yesno 'Delete the build files now?' n; then CLEANUP=yes; else CLEANUP=no; fi
  fi
  if [ "$CLEANUP" = yes ]; then
    for p in "${JUNK[@]}"; do rm -rf "$p"; say "    deleted $p"; done
    say "    freed $gb GB"
  else
    say "    build files kept ($gb GB)"
  fi
fi
exit 0
