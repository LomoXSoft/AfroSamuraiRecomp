#!/usr/bin/env python3
"""install.sh and its tools/linux helpers. No game files, network or Linux needed.

- install.sh keeps LF line endings, pins every download with a SHA-256, pins
  the MSVC CRT below 14.52 (its STL refuses Clang 20) and passes the Proton
  launch options the Windows Play.bat uses.
- It applies the same patch folder as tools/build_gpu_plugin.bat and checks the
  same two patch markers as tools/install.ps1.
- sync_tree.py writes only changed files, removes stale ones and leaves the
  kept paths alone.
- xwin_case_links.py adds a link for a header the sources spell in another case.
- make_release_assets.py writes a .desktop launcher whose Exec is one
  `bash -c '...'` argument with no field codes, escapes or inner quotes, that
  checks the tarball's SHA-256 and never names afrorecomp.exe (install.sh
  would take its own launcher for the running game).
"""
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
SH = ROOT / "install.sh"
LINUX = ROOT / "tools" / "linux"


def fail(msg):
    print("FAIL:", msg, file=sys.stderr)
    raise SystemExit(1)


def check_script():
    raw = SH.read_bytes()
    if b"\r\n" in raw:
        fail("install.sh has CRLF line endings (bash on Linux cannot run it)")
    if not raw.startswith(b"#!/usr/bin/env bash\n"):
        fail("install.sh must start with #!/usr/bin/env bash")
    text = raw.decode("utf-8")
    entries = re.findall(r'^\s*"([a-z-]+)\|((?:https://|\$NIGHTLY/)[^|"]+)\|([^|"]*)\|([^|"]+)"$', text, re.M)
    names = {e[0] for e in entries}
    for need in ("llvm", "cmake", "ninja", "xwin", "pycryptodome", "sdk-linux", "sdk-win"):
        if need not in names:
            fail("install.sh has no pinned download named " + need)
    for name, url, sha, _ in entries:
        if not re.fullmatch(r"[0-9a-f]{64}", sha):
            fail("download %s has no SHA-256 pin" % name)
    m = re.search(r"^XWIN_CRT=14\.(\d+)\.", text, re.M)
    if not m or int(m.group(1)) >= 52:
        fail("XWIN_CRT must pin an MSVC CRT below 14.52 (Clang 20)")
    ps1 = (ROOT / "tools" / "install.ps1").read_text(encoding="utf-8", errors="ignore")
    commit = re.search(r"\$REXGLUE_COMMIT = '(\w+)'", ps1).group(1)
    if "REXGLUE_COMMIT=%s\n" % commit not in text:
        fail("install.sh must build the same ReXGlue commit as install.ps1 (%s)" % commit)
    if "nightly-" not in text or commit not in re.search(r"^NIGHTLY=(.*)$", text, re.M).group(1):
        fail("the ReXGlue nightly in install.sh must be the one for commit " + commit)
    for marker in ("afro_xma_frameless_chain", "afro_edram_no_wrap"):
        if marker not in text or marker not in ps1:
            fail("install.sh must verify the patch marker " + marker)
    if '"$REPO/patches/rexglue/"*.patch' not in text:
        fail("install.sh must apply patches/rexglue/*.patch")
    if "zzz-afro-quad-depth-offset.patch" not in text:
        fail("install.sh lost the quad-depth special case of build_gpu_plugin.bat")
    opts = '--game_data_root="../gamedata" --gpu_plugin=xenos --log_file="../afro.log"'
    if opts not in text:
        fail("install.sh must print the Proton launch options " + opts)
    for keep in ("app/out", "app/generated/default", "app/assets", "app/afro_switch_tables.toml", "app/region"):
        if keep not in text:
            fail("install.sh source sync must keep " + keep)
    if os.name != "nt" and shutil.which("bash"):  # on Windows, bash may be WSL's
        r = subprocess.run(["bash", "-n", str(SH)], capture_output=True, text=True)
        if r.returncode:
            fail("bash -n install.sh: " + r.stderr.strip())
    # Git on Windows does not see the executable bit: it must be in the index,
    # or a Linux clone gets "Permission denied" for ./install.sh.
    if shutil.which("git"):
        r = subprocess.run(["git", "-C", str(ROOT), "ls-files", "-s", "install.sh"],
                           capture_output=True, text=True)
        if r.returncode == 0 and r.stdout and not r.stdout.startswith("100755"):
            fail("install.sh is not executable in git (git update-index --chmod=+x install.sh)")
    cmake = (LINUX / "win-cross.cmake").read_text(encoding="utf-8")
    for need in ("CMAKE_SYSTEM_NAME Windows", "x86_64-pc-windows-msvc", "CMAKE_AR llvm-ar",
                 "CMAKE_TRY_COMPILE_CONFIGURATION Release"):
        if need not in cmake:
            fail("win-cross.cmake lacks " + need)


def check_sync():
    tmp = pathlib.Path(tempfile.mkdtemp())
    try:
        src, dst = tmp / "src", tmp / "dst"
        (src / "app" / "src").mkdir(parents=True)
        (src / "app" / "src" / "a.cpp").write_text("one")
        (src / "app" / "src" / "b.cpp").write_text("two")
        (src / "game.iso").write_text("disc")
        (dst / "app" / "src").mkdir(parents=True)
        (dst / "app" / "out").mkdir(parents=True)
        (dst / "app" / "src" / "a.cpp").write_text("one")
        (dst / "app" / "src" / "old.cpp").write_text("stale")
        (dst / "app" / "out" / "obj.o").write_text("build")
        old = time.time() - 1000
        os.utime(dst / "app" / "src" / "a.cpp", (old, old))
        r = subprocess.run([sys.executable, str(LINUX / "sync_tree.py"), str(src), str(dst), "app/out"],
                           capture_output=True, text=True)
        if r.returncode:
            fail("sync_tree.py: " + r.stderr)
        if "1 changed, 1 unchanged, 1 removed" not in r.stdout:
            fail("sync_tree.py counted wrong: " + r.stdout.strip())
        if (dst / "app" / "src" / "a.cpp").stat().st_mtime > old + 1:
            fail("sync_tree.py rewrote an unchanged file (Ninja would recompile it)")
        if (dst / "app" / "src" / "b.cpp").read_text() != "two":
            fail("sync_tree.py did not copy a new file")
        if (dst / "app" / "src" / "old.cpp").exists():
            fail("sync_tree.py kept a file the source no longer has")
        if not (dst / "app" / "out" / "obj.o").exists():
            fail("sync_tree.py touched a kept path")
        if (dst / "game.iso").exists():
            fail("sync_tree.py copied a disc image")
    finally:
        shutil.rmtree(tmp)


def check_case_links():
    if not hasattr(os, "symlink") or os.name == "nt":
        print("skip: case links need POSIX symlinks")
        return
    tmp = pathlib.Path(tempfile.mkdtemp())
    try:
        xwin, src = tmp / "xwin", tmp / "src"
        um = xwin / "sdk" / "include" / "um"
        um.mkdir(parents=True)
        (um / "objbase.h").write_text("//")
        lib = xwin / "sdk" / "lib" / "um" / "x86_64"
        lib.mkdir(parents=True)
        (lib / "dbghelp.lib").write_text("lib")
        src.mkdir()
        (src / "p.cpp").write_text('#include <ObjBase.h>\n#pragma comment(lib, "DbgHelp.lib")\n')
        r = subprocess.run([sys.executable, str(LINUX / "xwin_case_links.py"), str(xwin), str(src)],
                           capture_output=True, text=True)
        if r.returncode:
            fail("xwin_case_links.py: " + r.stderr)
        if not (um / "ObjBase.h").is_file() or not (lib / "DbgHelp.lib").is_file():
            fail("xwin_case_links.py did not link ObjBase.h and DbgHelp.lib")
    finally:
        shutil.rmtree(tmp)


def check_release_assets():
    import importlib.util
    import shlex
    spec = importlib.util.spec_from_file_location("mra", LINUX / "make_release_assets.py")
    mra = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mra)
    sha = "ab" * 32
    text = mra.desktop_text("9.9.9", "afrorecomp-linux-v9.9.9.tar.gz", sha)
    lines = text.splitlines()
    if lines[0] != "[Desktop Entry]" or "Terminal=true" not in lines or "Type=Application" not in lines:
        fail("launcher is not a terminal desktop entry")
    exec_line = next(l for l in lines if l.startswith("Exec="))[5:]
    argv = shlex.split(exec_line)
    if argv[:2] != ["bash", "-c"] or len(argv) != 3:
        fail(f"launcher Exec does not split into bash -c <script>: {argv[:3]}")
    script = argv[2]
    for bad in ("%", "\\", "afrorecomp.exe"):
        if bad in exec_line:
            fail(f"launcher Exec contains {bad!r}")
    url = "https://github.com/LomoXSoft/AfroSamuraiRecomp/releases/download/v9.9.9/afrorecomp-linux-v9.9.9.tar.gz"
    if url not in script or f'echo "{sha}  $W/$T" | sha256sum -c -' not in script:
        fail("launcher does not download the tag's tarball and check its SHA-256")
    if '--update --dir "$D"' not in script or 'read -r -p' not in script:
        fail("launcher lost the update path or the final pause")


def main():
    check_script()
    check_sync()
    check_case_links()
    check_release_assets()
    print("ok: install.sh and tools/linux")


if __name__ == "__main__":
    main()
