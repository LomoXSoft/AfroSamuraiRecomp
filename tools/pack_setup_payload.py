#!/usr/bin/env python3
"""Encode the source snapshot that afrosetup.exe carries.

--out writes the payload file that afrosetup.rc embeds as RCDATA resource 101
(the release build: run this before llvm-rc). --exe appends it to an exe as
an overlay with a footer instead (the old layout, kept for comparison builds).
The setup extracts the files only after the user presses Install.
"""

from __future__ import annotations

import argparse
import pathlib
import struct

MAGIC = b"AFSTP001"


def ok_path(path: str) -> bool:
    if not path or path.startswith(("/", "\\")) or "\\" in path:
        return False
    for seg in path.split("/"):
        if seg in ("", ".", ".."):
            return False
        if any(not (c.isalnum() or c in "._-+") for c in seg):
            return False
    return True


# rexglue codegen writes these back with LF. A CRLF copy from a Windows checkout
# made every update differ from the installed file and run the codegen again.
LF_FILES = {"app/afrorecomp_manifest.toml"}


def collect(repo: pathlib.Path, overlay: pathlib.Path) -> list[tuple[str, bytes]]:
    skip_dirs = {".git"}
    skip_suffix = {".iso", ".xex", ".pe", ".exe", ".res", ".o", ".obj"}
    files: dict[str, pathlib.Path] = {}
    for root in (repo, overlay):
        for path in root.rglob("*"):
            if not path.is_file():
                continue
            rel = path.relative_to(root).as_posix()
            parts = set(path.relative_to(root).parts)
            if parts & skip_dirs:
                continue
            if path.suffix.lower() in skip_suffix and path.name != "install.bat":
                continue
            if path.name in ("afrosetup.exe", "afrosetup_payload.bin") or (
                path.name.startswith("afrosetup-v") and path.suffix.lower() == ".exe"
            ):
                continue
            files[rel] = path
    out = []
    for rel in sorted(files):
        if not ok_path(rel):
            raise SystemExit(f"unsafe path: {rel}")
        data = files[rel].read_bytes()
        if rel in LF_FILES:
            data = data.replace(b"\r\n", b"\n")
        out.append((rel, data))
    return out


def encode(files: list[tuple[str, bytes]]) -> bytes:
    body = struct.pack("<I", len(files))
    for rel, data in files:
        raw = rel.encode("utf-8")
        body += struct.pack("<H", len(raw)) + raw + struct.pack("<Q", len(data)) + data
    return body


def main() -> None:
    parser = argparse.ArgumentParser()
    target = parser.add_mutually_exclusive_group(required=True)
    target.add_argument("--out", type=pathlib.Path, help="write the RCDATA payload file")
    target.add_argument("--exe", type=pathlib.Path, help="append the payload to this exe")
    parser.add_argument("--repo", type=pathlib.Path, required=True)
    parser.add_argument("--overlay", type=pathlib.Path, required=True)
    args = parser.parse_args()
    files = collect(args.repo, args.overlay)
    names = {rel for rel, _ in files}
    for need in ("app/VERSION", "tools/install.ps1", "tools/build_win.bat", "install.bat", "app/afro_hooks.toml"):
        if need not in names:
            raise SystemExit(f"snapshot missing {need}")
    version = dict(files)["app/VERSION"].decode("utf-8").strip()
    if version != "0.3.8":
        raise SystemExit(f"snapshot VERSION is {version!r}")
    ps1 = dict(files)["tools/install.ps1"].decode("utf-8")
    if "Refusing -Ref main" not in ps1 or "$LOCAL_TREE_VERSION = '0.3.8'" not in ps1:
        raise SystemExit("snapshot install.ps1 is not the local-tree build")
    body = encode(files)
    if args.out:
        args.out.write_bytes(body)
        print(f"payload files: {len(files)}")
        print(f"payload bytes: {len(body)} (resource file {args.out.name})")
        return
    exe = args.exe.read_bytes()
    if exe[:2] != b"MZ":
        raise SystemExit("exe is not a PE")
    footer = struct.pack("<QQ", len(exe), len(body)) + MAGIC
    args.exe.write_bytes(exe + body + footer)
    print(f"payload files: {len(files)}")
    print(f"payload bytes: {len(body)}")
    print(f"exe bytes: {args.exe.stat().st_size}")


if __name__ == "__main__":
    main()
