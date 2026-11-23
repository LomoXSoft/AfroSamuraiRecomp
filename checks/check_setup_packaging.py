#!/usr/bin/env python3
"""Packaging checks for tools/afrosetup.exe. No Windows install is performed."""

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


def sections(blob: bytes, pe: int) -> list[tuple[int, int, int, int]]:
    """(virtual address, virtual size, raw offset, raw size) of each section."""
    count = struct.unpack_from("<H", blob, pe + 6)[0]
    opt_size = struct.unpack_from("<H", blob, pe + 20)[0]
    first = pe + 24 + opt_size
    out = []
    for i in range(count):
        va_size, va, raw_size, raw = struct.unpack_from("<IIII", blob, first + 40 * i + 8)
        out.append((va, va_size, raw, raw_size))
    return out


def rva_to_offset(secs: list[tuple[int, int, int, int]], rva: int) -> int:
    for va, va_size, raw, raw_size in secs:
        if va <= rva < va + max(va_size, raw_size):
            return raw + (rva - va)
    fail(f"RVA {rva:#x} is in no section")
    return 0


def rcdata(blob: bytes, pe: int, res_id: int) -> tuple[int, int]:
    """File offset and size of RT_RCDATA resource `res_id` (first language)."""
    rsrc_rva = struct.unpack_from("<I", blob, pe + 24 + 112 + 2 * 8)[0]
    if not rsrc_rva:
        fail("PE has no resource directory")
    secs = sections(blob, pe)
    base = rva_to_offset(secs, rsrc_rva)

    def entries(off: int) -> list[tuple[int, int]]:
        named, ids = struct.unpack_from("<HH", blob, off + 12)
        return [struct.unpack_from("<II", blob, off + 16 + 8 * i) for i in range(named + ids)]

    def child(off: int, want: int | None) -> int:
        for name, target in entries(off):
            if want is None or (not name & 0x80000000 and name == want):
                return target
        fail(f"resource {want} not found")
        return 0

    t = child(base, 10)  # RT_RCDATA
    n = child(base + (t & 0x7FFFFFFF), res_id)
    leaf = child(base + (n & 0x7FFFFFFF), None)
    if leaf & 0x80000000:
        fail("resource leaf is a directory")
    data_rva, size = struct.unpack_from("<II", blob, base + leaf)[:2]
    return rva_to_offset(secs, data_rva), size


def payload_files(body: bytes) -> dict[str, bytes]:
    count = struct.unpack_from("<I", body, 0)[0]
    pos, out = 4, {}
    for _ in range(count):
        n = struct.unpack_from("<H", body, pos)[0]
        rel = body[pos + 2 : pos + 2 + n].decode("utf-8")
        pos += 2 + n
        size = struct.unpack_from("<Q", body, pos)[0]
        pos += 8
        out[rel] = body[pos : pos + size]
        pos += size
    if pos != len(body):
        fail("payload has trailing bytes")
    return out


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
    # No overlay: the file ends with the last section (or with an Authenticode
    # certificate, once the setup is signed). The payload is RCDATA 101.
    secs = sections(blob, pe)
    image_end = max(raw + raw_size for _, _, raw, raw_size in secs)
    cert_off, cert_size = struct.unpack_from("<II", blob, pe + 24 + 112 + 4 * 8)
    if len(blob) != image_end and not (cert_off == image_end and cert_off + cert_size == len(blob)):
        fail(f"{len(blob) - image_end} bytes follow the PE image (overlay)")
    if blob[-8:] == MAGIC:
        fail("old overlay footer present")
    off, size = rcdata(blob, pe, 101)
    files = payload_files(blob[off : off + size])
    for need in ("app/VERSION", "tools/install.ps1", "tools/build_win.bat", "install.bat", "app/afro_hooks.toml"):
        if need not in files:
            fail(f"payload is missing {need}")
    # Search the program, not the payload (the payload holds this very check's text).
    image = blob[:off] + blob[off + size :]
    if b"requireAdministrator" in image:
        fail("PE image requests administrator")
    if b"asInvoker" not in image:
        fail("asInvoker manifest missing from the PE image")
    cpp = (ROOT / "tools" / "afrosetup.cpp").read_text(encoding="utf-8")
    ps1 = (ROOT / "tools" / "install.ps1").read_text(encoding="utf-8")
    if "ConfirmedStop" not in cpp or "TerminateJobObject" not in cpp:
        fail("cancel is not limited to the process tree of this run")
    # Antivirus heuristics: no process enumeration or opening other processes,
    # and install.ps1 runs under RemoteSigned, not Bypass.
    for name in (b"CreateToolhelp32Snapshot", b"Process32FirstW", b"OpenProcess"):
        if name in image:
            fail(f"setup imports {name.decode()}")
    if "Bypass".encode("utf-16-le") in image:
        fail("setup command line lowers the execution policy to Bypass")
    if "RemoteSigned".encode("utf-16-le") not in image:
        fail("setup command line lost -ExecutionPolicy RemoteSigned")
    if "taskkill" in cpp.lower() or "Stop-Process" in ps1:
        fail("cancel kills by image name")
    if "[string]$Ref = 'main'" not in ps1:
        fail("console default branch was removed")
    if "Refusing -Ref main" not in ps1 or "$LOCAL_TREE_VERSION = '0.3.8'" not in ps1:
        fail("local-tree guard missing")
    if "logwrite.ps1" not in ps1 or "Add-Content" in ps1:
        fail("install.ps1 still appends the log with Add-Content")
    logwrite = (ROOT / "tools" / "logwrite.ps1").read_text(encoding="utf-8")
    if "FileShare]::ReadWrite" not in logwrite or "LogBusy" not in logwrite:
        fail("log writer is missing the share mode or the recursion guard")
    version = (ROOT / "app" / "VERSION").read_text(encoding="utf-8").strip()
    if version != "0.3.8":
        fail(f"VERSION is {version!r}")
    if b"Lomorsin Afro header 1200x240" not in image:
        fail("setup image is missing the header art marker")
    icon = (ROOT / "tools" / "artwork" / "lomorsin-afro" / "icono_32.png").read_bytes()
    if icon not in image:
        fail("setup image is missing the supplied 32px icon")
    # Version info (Properties > Details) and no suspended child process: an
    # anonymous unsigned exe that starts a hidden process suspended and then
    # resumes it is what antivirus heuristics flag as a dropper.
    for text in ("LomoXSoft", "AfroSamuraiRecomp Setup", f"afrosetup-v{version}.exe"):
        if text.encode("utf-16-le") not in image:
            fail(f"version info is missing {text!r}")
    if "CREATE_SUSPENDED" in cpp or "ResumeThread" in cpp:
        fail("setup starts a process suspended")
    if files["app/VERSION"].decode("utf-8").strip() != version:
        fail("payload app/VERSION differs from app/VERSION")
    rc = (ROOT / "tools" / "afrosetup.rc").read_text(encoding="utf-8")
    hdr = (ROOT / "tools" / "afrosetup_cmd.h").read_text(encoding="utf-8")
    if '101 RCDATA "afrosetup_payload.bin"' not in rc or "kPayloadResourceId = 101;" not in hdr:
        fail("payload resource id differs between afrosetup.rc and afrosetup_cmd.h")
    if "FindResourceW(nullptr, MAKEINTRESOURCEW(afrosetup::kPayloadResourceId)" not in cpp or "_wfopen_s(&f, path, L\"rb\")" in cpp:
        fail("setup does not read its payload from the resource")
    print("setup packaging checks: ok")
    print(f"  {EXE}  {len(blob)} bytes  amd64 GUI  asInvoker")
    print(f"  payload: RCDATA 101 at {off}, {size} bytes, {len(files)} files, no overlay")
    print("  no Windows install was run")


if __name__ == "__main__":
    main()
