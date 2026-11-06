#!/usr/bin/env python3
"""The update stamp table in install.ps1 matches the C++ contract."""

from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
ps1 = (root / "tools" / "install.ps1").read_text(encoding="utf-8")
header = (root / "tools" / "afrosetup_cmd.h").read_text(encoding="utf-8")
cpp = (root / "tools" / "afrosetup.cpp").read_text(encoding="utf-8")


def fail(msg: str) -> None:
    print("FAIL:", msg, file=sys.stderr)
    raise SystemExit(1)


pairs = [
    ("git", r"tools\git\cmd\git.exe"),
    ("cmake", r"tools\cmake\bin\cmake.exe"),
    ("ninja", r"tools\ninja\ninja.exe"),
    ("python", r"tools\python\python.exe"),
    ("llvm", r"tools\llvm\bin\clang++.exe"),
    ("msvc", r"tools\msvc\setup_x64.bat"),
    ("sdk", r"sdk\win-amd64\bin\rexglue.exe"),
    ("rexglue-src", r"rexglue-sdk\.git"),
    ("gamedata", r"gamedata\default.xex"),
]
for stamp, rel in pairs:
    if f"'{stamp}'" not in ps1 or rel not in ps1:
        fail(f"install.ps1 missing reuse rule {stamp} -> {rel}")
    slash = rel.replace("\\", "/").replace(r"\.git", "/.git")
    if slash not in header and rel.replace("\\", "/") not in header:
        fail(f"header missing {slash}")
for stamp in ("build-app", "build-gpu", "build"):
    if f"'{stamp}'" not in ps1 or f'"{stamp}"' not in header:
        fail(f"rebuild stamp {stamp} missing")
for required in (
    "afrorecomp.exe is running",
    "will not stop it",
    "afro_language.cfg not rewritten",
    "Previous program files were put back",
    "gamedata",
    "[switch]$Update",
):
    if required not in ps1:
        fail(f"install.ps1 missing {required}")
if "TerminateProcess" in cpp:
    fail("setup UI can kill a process")
if "kSource" in cpp:
    fail("source choice page returned")
print("update contract: ok")
