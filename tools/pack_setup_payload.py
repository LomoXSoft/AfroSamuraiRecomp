#!/usr/bin/env python3
"""Append the 0.1.0 source snapshot to afrosetup.exe.

The Windows loader ignores this overlay. The setup program reads the footer
and extracts the files only after the user presses Install.
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
            if path.name == "afrosetup.exe":
                continue
            files[rel] = path
    out = []
    for rel in sorted(files):
        if not ok_path(rel):
            raise SystemExit(f"unsafe path: {rel}")
        out.append((rel, files[rel].read_bytes()))
    return out


def encode(files: list[tuple[str, bytes]]) -> bytes:
    body = struct.pack("<I", len(files))
    for rel, data in files:
        raw = rel.encode("utf-8")
        body += struct.pack("<H", len(raw)) + raw + struct.pack("<Q", len(data)) + data
    return body


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=pathlib.Path, required=True)
    parser.add_argument("--repo", type=pathlib.Path, required=True)
    parser.add_argument("--overlay", type=pathlib.Path, required=True)
    args = parser.parse_args()
    files = collect(args.repo, args.overlay)
    names = {rel for rel, _ in files}
    for need in ("app/VERSION", "tools/install.ps1", "tools/build_win.bat", "install.bat", "app/afro_hooks.toml"):
        if need not in names:
            raise SystemExit(f"snapshot missing {need}")
    version = dict(files)["app/VERSION"].decode("utf-8").strip()
    if version != "0.1.0":
        raise SystemExit(f"snapshot VERSION is {version!r}")
    ps1 = dict(files)["tools/install.ps1"].decode("utf-8")
    if "Refusing -Ref main" not in ps1 or "$LOCAL_TREE_VERSION = '0.1.0'" not in ps1:
        raise SystemExit("snapshot install.ps1 is not the local-tree build")
    body = encode(files)
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
