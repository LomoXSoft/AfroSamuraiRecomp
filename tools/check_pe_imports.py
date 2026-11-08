#!/usr/bin/env python3
"""Resolve the DLLs a Windows EXE imports before it is started.

A missing DLL makes the loader exit before main, with a dialog and no
stdout. The build log then only shows a rewritten exit code. This prints
the missing name, and can copy the matching CRT DLLs from the MSVC bin
(the one setup_x64.bat puts on PATH) next to the EXE so that directory is
found first.
"""
import argparse
import shutil
import struct
import sys
from pathlib import Path

# API sets are not files. These Windows DLLs live in System32.
SYSTEM = {
    "kernel32.dll", "kernelbase.dll", "ntdll.dll", "user32.dll", "advapi32.dll",
    "shell32.dll", "ole32.dll", "gdi32.dll", "ws2_32.dll", "bcrypt.dll",
    "dxgi.dll", "imm32.dll", "winmm.dll", "setupapi.dll", "version.dll",
    "oleaut32.dll", "comctl32.dll", "comdlg32.dll", "shlwapi.dll",
}


def _u16(b, o):
    return struct.unpack_from("<H", b, o)[0]


def _u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def _rva_to_off(sections, rva):
    for va, vsize, raw, rawsize in sections:
        span = max(vsize, rawsize)
        if va <= rva < va + span:
            return raw + (rva - va)
    raise ValueError(f"RVA 0x{rva:x} is outside the image")


def pe_import_dlls(path):
    """DLL names from the PE import directory. Delay-loads are not included."""
    data = Path(path).read_bytes()
    if data[:2] != b"MZ":
        raise ValueError(f"not a PE: {path}")
    e = _u32(data, 0x3C)
    if data[e:e + 4] != b"PE\0\0":
        raise ValueError(f"not a PE: {path}")
    nsec = _u16(data, e + 6)
    opt = _u16(data, e + 20)
    magic = _u16(data, e + 24)
    dd = e + 24 + (112 if magic == 0x20B else 96)
    imp_rva = _u32(data, dd + 8)
    sections = []
    sec = e + 24 + opt
    for i in range(nsec):
        o = sec + i * 40
        sections.append((_u32(data, o + 12), _u32(data, o + 8), _u32(data, o + 20), _u32(data, o + 16)))
    if imp_rva == 0:
        return []
    off = _rva_to_off(sections, imp_rva)
    names = []
    while True:
        name_rva = _u32(data, off + 12)
        if name_rva == 0 and _u32(data, off) == 0:
            break
        if name_rva:
            no = _rva_to_off(sections, name_rva)
            names.append(data[no:data.index(b"\0", no)].decode("ascii"))
        off += 20
    return names


def is_system_dll(name):
    low = name.lower()
    return low.startswith("api-ms-win-") or low.startswith("ext-ms-") or low in SYSTEM


def missing_imports(dll_names, search_dirs):
    """Names that are not system DLLs and are not in any search directory."""
    found = set()
    for directory in search_dirs:
        d = Path(directory)
        if not d.is_dir():
            continue
        for child in d.iterdir():
            if child.is_file():
                found.add(child.name.lower())
    missing = []
    for name in dll_names:
        if is_system_dll(name):
            continue
        if name.lower() not in found:
            missing.append(name)
    return missing


def stage_matching(dll_names, exe_dir, donors):
    """Copy donor copies of imported DLLs next to the EXE. Returns copied names."""
    wanted = {n.lower(): n for n in dll_names if not is_system_dll(n)}
    copied = []
    dest = Path(exe_dir)
    for donor in donors:
        d = Path(donor)
        if not d.is_dir():
            continue
        for child in d.iterdir():
            key = child.name.lower()
            if child.is_file() and key in wanted:
                target = dest / child.name
                if not target.exists() or target.read_bytes() != child.read_bytes():
                    shutil.copy2(child, target)
                copied.append(child.name)
                del wanted[key]
    return copied


def closure(start, search_dirs):
    """Imports of start plus imports of non-system DLLs found beside it."""
    seen = set()
    order = []
    pending = [Path(start)]
    while pending:
        path = pending.pop(0)
        key = str(path.resolve()).lower()
        if key in seen or not path.is_file():
            continue
        seen.add(key)
        try:
            names = pe_import_dlls(path)
        except ValueError:
            continue
        for name in names:
            if name.lower() not in {n.lower() for n in order}:
                order.append(name)
            if is_system_dll(name):
                continue
            for directory in search_dirs:
                candidate = Path(directory) / name
                if candidate.is_file():
                    pending.append(candidate)
                    break
    return order


def main(argv=None):
    p = argparse.ArgumentParser()
    p.add_argument("exe")
    p.add_argument("--stage-from", action="append", default=[])
    args = p.parse_args(argv)
    exe = Path(args.exe)
    exe_dir = exe.parent
    donors = [Path(x) for x in args.stage_from if x]
    if donors:
        first = closure(exe, [exe_dir] + donors)
        copied = stage_matching(first, exe_dir, donors)
        for name in copied:
            print(f"staged {name}")
    names = closure(exe, [exe_dir] + donors)
    missing = missing_imports(names, [exe_dir])
    if missing:
        for name in missing:
            print(f"missing DLL: {name} (not beside {exe.name})")
        return 2
    print(f"imports ok: {exe.name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
