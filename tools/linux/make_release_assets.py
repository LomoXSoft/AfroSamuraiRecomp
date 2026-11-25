#!/usr/bin/env python3
"""Build the two Linux release assets for one tag. Nothing is uploaded.

  afrorecomp-linux-v<ver>.tar.gz
      `git archive` of the tag with LF line endings and the prefix
      afrorecomp-v<ver>/, the same files a `git clone` of that tag gives.
  AfroSamuraiRecomp-Linux-v<ver>.desktop
      A double-clickable launcher (KDE on SteamOS, GNOME, ...). It opens a
      terminal, downloads that tarball from the tag's GitHub release, checks
      its SHA-256 (written in here), unpacks it to a temporary folder and runs
      its install.sh: with --update when ~/Games/AfroSamuraiRecomp is already
      an installation (afrorecomp/app/VERSION), else as a new install that asks
      its questions. It waits for
      Enter at the end so the window stays readable.

usage: python3 tools/linux/make_release_assets.py --ref v0.3.11 --out <dir>
"""

from __future__ import annotations

import argparse
import hashlib
import pathlib
import re
import subprocess

REPO_URL = "https://github.com/LomoXSoft/AfroSamuraiRecomp"
ROOT = pathlib.Path(__file__).resolve().parents[2]


def fail(msg: str) -> None:
    raise SystemExit(f"linux assets: {msg}")


def desktop_text(ver: str, tarball: str, sha: str) -> str:
    url = f"{REPO_URL}/releases/download/v{ver}/{tarball}"
    src = f"afrorecomp-v{ver}"
    # One bash -c argument in single quotes, as other SteamOS installers ship it.
    # Desktop-entry rules: no % (field codes), no backslash (escapes), and no
    # single quote inside the script.
    script = "; ".join([
        "W=$(mktemp -d)",
        f"T={tarball}",
        "D=$HOME/Games/AfroSamuraiRecomp",
        f'echo "AfroSamuraiRecomp v{ver} for Linux and SteamOS (Proton)"',
        "echo",
        f'if curl -fL --retry 3 -o "$W/$T" {url} '
        f'&& echo "{sha}  $W/$T" | sha256sum -c - '
        f'&& tar -xzf "$W/$T" -C "$W"; then '
        f'if [ -f "$D/afrorecomp/app/VERSION" ]; then '
        f'echo "Updating $D"; bash "$W/{src}/install.sh" --update --dir "$D"; '
        f'else bash "$W/{src}/install.sh"; fi; '
        'else echo "The download or its SHA-256 check failed. Nothing was installed."; fi',
        "echo",
        'read -r -p "Press Enter to close this window. " x',
    ])
    for bad in ("%", "\\", "'"):
        if bad in script:
            fail(f"launcher script contains {bad!r}")
    # install.sh refuses to update while `pgrep -f 'afrorecomp\.exe'` finds a
    # process; this command line must not match it.
    if "afrorecomp.exe" in script:
        fail("launcher script names afrorecomp.exe, which install.sh would see as the running game")
    return "\n".join([
        "[Desktop Entry]",
        "Type=Application",
        f"Name=Install Afro Samurai Recomp v{ver}",
        "Comment=Builds the Windows game from your own disc and sets it up for Proton",
        "Icon=applications-games",
        "Terminal=true",
        "StartupNotify=false",
        f"Exec=bash -c '{script}'",
        "",
    ])


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ref", required=True, help="release tag, for example v0.3.11")
    ap.add_argument("--out", type=pathlib.Path, required=True)
    args = ap.parse_args()
    m = re.fullmatch(r"v(\d+(?:\.\d+)+)", args.ref)
    if not m:
        fail("--ref must be a release tag like v0.3.11")
    ver = m.group(1)

    def git(*a: str) -> bytes:
        return subprocess.run(["git", "-C", str(ROOT), *a], check=True, capture_output=True).stdout

    tag_ver = git("show", f"{args.ref}:app/VERSION").decode().strip()
    if tag_ver != ver:
        fail(f"app/VERSION at {args.ref} is {tag_ver}")
    args.out.mkdir(parents=True, exist_ok=True)
    tarball = f"afrorecomp-linux-v{ver}.tar.gz"
    tar_path = args.out / tarball
    # LF everywhere, whatever this checkout's autocrlf says; .gitattributes still applies.
    git("-c", "core.autocrlf=false", "-c", "core.eol=lf", "archive", "--format=tar.gz",
        f"--prefix=afrorecomp-v{ver}/", "-o", str(tar_path.resolve()), args.ref)
    sha = hashlib.sha256(tar_path.read_bytes()).hexdigest()
    desk = args.out / f"AfroSamuraiRecomp-Linux-v{ver}.desktop"
    desk.write_bytes(desktop_text(ver, tarball, sha).encode("utf-8"))
    for p in (tar_path, desk):
        print(f"{p.name}  {p.stat().st_size} bytes  sha256 {hashlib.sha256(p.read_bytes()).hexdigest()}")


if __name__ == "__main__":
    main()
