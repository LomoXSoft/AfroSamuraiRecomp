#!/usr/bin/env python3
"""Static checks for the GitHub Actions setup build. Nothing is built or uploaded.

- The workflow starts by hand only and uses actions pinned to commit hashes.
- Its LLVM download is the one install.ps1 pins (same URL and SHA-256).
- It never changes a published release: the upload step is for drafts only.
- tools/build_release_setup.ps1 keeps the release recipe (static CRT, GUI
  subsystem, wWinMainCRTStartup, payload from a git archive export).
"""

from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
WORKFLOW = ROOT / ".github" / "workflows" / "setup.yml"
SCRIPT = ROOT / "tools" / "build_release_setup.ps1"
INSTALL = ROOT / "tools" / "install.ps1"


def fail(msg: str) -> None:
    print(f"FAIL: {msg}", file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    wf = WORKFLOW.read_text(encoding="utf-8")
    ps = SCRIPT.read_text(encoding="utf-8")
    install = INSTALL.read_text(encoding="utf-8")

    on = re.search(r"^on:\n((?:[ \t]+.*\n|\n)*)", wf, re.M)
    if not on:
        fail("workflow has no 'on:' block")
    triggers = re.findall(r"^  (\w+):", on.group(1), re.M)
    if triggers != ["workflow_dispatch"]:
        fail(f"workflow must start by hand only, triggers are {triggers}")

    uses = re.findall(r"uses:\s*(\S+)", wf)
    if not uses:
        fail("workflow uses no actions")
    for ref in uses:
        if not re.fullmatch(r"[\w.-]+/[\w.-]+@[0-9a-f]{40}", ref):
            fail(f"action not pinned to a commit hash: {ref}")

    llvm = re.search(r"llvm\s*=\s*@\{ url = '([^']+)'; sha = '([0-9a-f]{64})'", install)
    if not llvm:
        fail("install.ps1 LLVM pin not found")
    if f"LLVM_URL: {llvm.group(1)}" not in wf or f"LLVM_SHA256: {llvm.group(2)}" not in wf:
        fail("workflow LLVM URL or SHA-256 differs from install.ps1")
    if "Get-FileHash" not in wf or "$env:LLVM_SHA256" not in wf:
        fail("workflow does not verify the LLVM download")

    # windows-2025 ships MSVC 14.52, whose C++ library refuses Clang 20 (STL1000).
    if "runs-on: windows-2022" not in wf:
        fail("setup build must run on windows-2022 (MSVC 14.44 accepts Clang 20)")
    if "permissions: {}" not in wf:
        fail("workflow must default to no token permissions")
    if "persist-credentials: false" not in wf:
        fail("checkout keeps the token in .git/config")
    upload = wf.split("name: Attach to the draft release", 1)
    if len(upload) != 2:
        fail("draft release upload step missing")
    step = upload[1]
    if "if: inputs.upload" not in step or "isDraft" not in step or "already published" not in step:
        fail("upload step does not refuse published releases")
    if "${{ inputs." in "".join(re.findall(r"run: \|\n((?:[ \t]{10}.*\n)+)", wf)):
        fail("workflow inputs are expanded inside a script; pass them through env")
    if "tools/build_release_setup.ps1" not in wf or "check_setup_packaging.py" not in wf:
        fail("workflow does not run the build script and the packaging check")
    if "attest-build-provenance" not in wf:
        fail("build provenance attestation missing")

    for flag in (
        "'-fms-runtime-lib=static'",
        "'-Wl,/SUBSYSTEM:WINDOWS'",
        "'-Wl,/ENTRY:wWinMainCRTStartup'",
        "'-DWINVER=0x0A00'",
        "'-ladvapi32'",
    ):
        if flag not in ps:
            fail(f"build_release_setup.ps1 lost {flag}")
    for step_text in ("'archive'", "'core.autocrlf=true'", "make_setup_art.py", "llvm-rc", "pack_setup_payload.py"):
        if step_text not in ps:
            fail(f"build_release_setup.ps1 lost the {step_text} step")
    if re.search(r"gh\s+release|Invoke-WebRequest|git\s+push", ps, re.I):
        fail("build_release_setup.ps1 must not upload or download anything")

    print("setup workflow checks: ok")
    print(f"  {len(uses)} actions pinned, LLVM pin matches install.ps1, uploads to drafts only")


if __name__ == "__main__":
    main()
