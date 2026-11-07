#!/usr/bin/env python3
"""Packaging checks for tools/afrosetup-v<VERSION>.exe. No Windows install is performed."""

from __future__ import annotations

import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
VERSION = (ROOT / "app" / "VERSION").read_text(encoding="utf-8").strip()
EXE = ROOT / "tools" / f"afrosetup-v{VERSION}.exe"
MAGIC = b"AFSTP001"


def fail(msg: str) -> None:
    print(f"FAIL: {msg}", file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    if not EXE.is_file():
        fail(f"missing {EXE}")
    blob = EXE.read_bytes()
    if blob[:2] != b"MZ":
        fail("not an MZ executable")
    pe = struct.unpack_from("<I", blob, 0x3C)[0]
    if blob[pe : pe + 4] != b"PE\0\0":
        fail("PE signature missing")
    machine = struct.unpack_from("<H", blob, pe + 4)[0]
    if machine != 0x8664:
        fail(f"machine is {machine:#x}, expected amd64")
    if struct.unpack_from("<H", blob, pe + 24)[0] != 0x20B:
        fail("expected PE32+")
    subsystem = struct.unpack_from("<H", blob, pe + 24 + 68)[0]
    if subsystem != 2:
        fail(f"subsystem {subsystem}, expected Windows GUI (2)")
    off, size = struct.unpack_from("<QQ", blob, len(blob) - 24)
    if blob[-8:] != MAGIC:
        fail("payload magic missing")
    image = blob[:off]
    if b"requireAdministrator" in image:
        fail("PE image requests administrator")
    if b"asInvoker" not in image:
        fail("asInvoker manifest missing from the PE image")
    if b"TerminateProcess" in image:
        fail("PE image imports or names TerminateProcess")
    cpp = (ROOT / "tools" / "afrosetup.cpp").read_text(encoding="utf-8")
    if "TerminateProcess" in cpp or "JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE" in cpp:
        fail("setup source can kill the build")
    ps1 = (ROOT / "tools" / "install.ps1").read_text(encoding="utf-8")
    if "[string]$Ref = 'main'" not in ps1:
        fail("console default branch was removed")
    if "Refusing -Ref main" not in ps1 or "$LOCAL_TREE_VERSION = '0.1.0'" not in ps1:
        fail("local-tree guard missing")
    print("setup packaging checks: ok")
    print(f"  {EXE}  {len(blob)} bytes  amd64 GUI  asInvoker")
    print(f"  payload at {off} size {size}")
    print("  no Windows install was run")


if __name__ == "__main__":
    main()
