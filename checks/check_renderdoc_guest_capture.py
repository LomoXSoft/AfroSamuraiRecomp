#!/usr/bin/env python3
"""The guest-frame RenderDoc patch is opt-in and wraps the replay boundary.

It does not change shading. A Present-to-Present capture of this presenter is
only the bilinear blit; the patch must start before that frame's deferred
commands are replayed and end after the closing ExecuteCommandLists.
"""

from __future__ import annotations

import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
PATCH = ROOT / "patches" / "rexglue" / "zz-afro-renderdoc-guest-capture.patch"
PERF = ROOT / "patches" / "rexglue" / "zz-afro-perf-log.patch"
BAT = ROOT / "tools" / "build_gpu_plugin.bat"


def fail(msg: str) -> None:
    print(f"FAIL: {msg}", file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    if not PATCH.is_file():
        fail(f"missing {PATCH}")
    text = PATCH.read_text(encoding="utf-8")
    if not PERF.is_file():
        fail("perf patch missing; guest capture must apply after it")
    names = sorted(p.name for p in (ROOT / "patches" / "rexglue").glob("*.patch"))
    if names.index(PATCH.name) < names.index(PERF.name):
        fail("guest-capture patch sorts before zz-afro-perf-log.patch")
    bat = BAT.read_text(encoding="utf-8")
    if "patches\\rexglue\\*.patch" not in bat and "patches/rexglue/*.patch" not in bat:
        fail("GPU plugin build does not apply the rexglue patch set")
    required = [
        "afro_renderdoc_capture, 0",
        "afro_renderdoc_capture.request",
        "StartFrameCapture",
        "EndFrameCapture",
        "afro-guest-frame",
        "is_opening_frame",
        "is_closing_frame",
        "not attached",
    ]
    for item in required:
        if item not in text:
            fail(f"patch is missing {item!r}")
    if "TriggerCapture" in text:
        fail("patch uses TriggerCapture, which is another Present-bounded capture")
    # The one-shot must start when the guest frame opens, not only at Execute.
    start = text.find("g_afro_renderdoc.Begin")
    end = text.find("g_afro_renderdoc.End")
    open_at = text.find("is_opening_frame")
    close_at = text.find("is_closing_frame")
    if not (0 <= open_at < start < end and close_at < end):
        fail("Begin/End are not tied to the guest frame open and close")
    if "DrawInstanced(4, 1)" not in text:
        fail("patch does not record why the presenter blit is left outside")
    print("renderdoc guest-capture patch: ok")
    print(f"  {PATCH.name} applies after {PERF.name}")
    print("  one guest frame, off unless RenderDoc is attached and a request is made")


if __name__ == "__main__":
    main()
