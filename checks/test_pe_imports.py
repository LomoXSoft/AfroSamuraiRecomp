#!/usr/bin/env python3
"""The CRT DLLs rexglue imports, and that a donor copy satisfies the check."""
import shutil
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import check_pe_imports as pe

ROOT = Path("/tmp")
REX = ROOT / "rexglue.exe"
RUNTIME = ROOT / "rexruntime.dll"


def main():
    if not REX.is_file() or not RUNTIME.is_file():
        print("skip: rexglue.exe not extracted")
        return 0
    names = pe.pe_import_dlls(REX)
    runtime = pe.pe_import_dlls(RUNTIME)
    assert "rexruntime.dll" in names
    assert "MSVCP140.dll" in names
    assert "VCRUNTIME140.dll" in names
    assert "MSVCP140_ATOMIC_WAIT.dll" in runtime
    assert pe.missing_imports(["MSVCP140.dll", "KERNEL32.dll"], []) == ["MSVCP140.dll"]
    work = Path(tempfile.mkdtemp())
    try:
        exe = work / "bin"
        donor = work / "msvc"
        exe.mkdir()
        donor.mkdir()
        shutil.copy(REX, exe / "rexglue.exe")
        shutil.copy(RUNTIME, exe / "rexruntime.dll")
        for name, payload in (
            ("MSVCP140.dll", b"msvcp"),
            ("MSVCP140_ATOMIC_WAIT.dll", b"atom"),
            ("VCRUNTIME140.dll", b"vcr"),
            ("VCRUNTIME140_1.dll", b"vcr1"),
        ):
            (donor / name).write_bytes(payload)
        code = pe.main([str(exe / "rexglue.exe"), "--stage-from", str(donor)])
        assert code == 0, code
        assert (exe / "MSVCP140_ATOMIC_WAIT.dll").read_bytes() == b"atom"
        (exe / "MSVCP140_ATOMIC_WAIT.dll").unlink()
        missing = pe.missing_imports(pe.closure(exe / "rexglue.exe", [exe]), [exe])
        assert any("ATOMIC_WAIT" in item.upper() for item in missing)
    finally:
        shutil.rmtree(work)
    print("pe import checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
