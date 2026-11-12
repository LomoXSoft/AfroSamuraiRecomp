#!/usr/bin/env python3
"""The USA address map covers every Europe address the project uses. No game files needed.

A Europe address added to app/afro_hooks.toml, app/afro_functions.toml or an
AfroGameAddr(...) call without a line in app/regions/usa.toml would only fail
on a USA install; this check fails first.
"""
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import region_config as rc  # noqa: E402

APP = ROOT / "app"
ADDR = re.compile(r"0x8[23][0-9A-Fa-f]{6}")
STRING = re.compile(r'"(?:[^"' + "\\\\" + r']|' + "\\\\" + r'.)*"')


def fail(msg: str) -> None:
    print("FAIL:", msg, file=sys.stderr)
    raise SystemExit(1)


def code_addresses(toml_text: str) -> set[int]:
    out = set()
    for line in toml_text.splitlines():
        out.update(int(a, 16) for a in ADDR.findall(line.split("#", 1)[0]))
    return out


def main() -> None:
    usa = rc.load_map(APP, "usa")
    if not usa:
        fail("app/regions/usa.toml is empty")
    if rc.load_map(APP, "eu") is not None:
        fail("Europe must use the addresses as written")

    for name in ("afro_hooks.toml", "afro_functions.toml"):
        text = (APP / name).read_text(encoding="utf-8")
        for a in sorted(code_addresses(text)):
            if not usa.get(a):
                fail(f"{name}: 0x{a:08X} has no USA address")
        moved = rc.move_toml(text, usa, name)
        if code_addresses(moved) != {usa[a] for a in code_addresses(text)}:
            fail(f"{name}: the USA copy does not use exactly the mapped addresses")
        if rc.move_toml(text, None, name) != text:
            fail(f"{name}: the Europe copy is not unchanged")

    calls = 0
    for src in sorted((APP / "src").glob("*.[ch]*")):
        for n, line in enumerate(src.read_text(encoding="utf-8").splitlines(), 1):
            code = STRING.sub('""', line.split("//", 1)[0])
            for m in rc.CALL.finditer(code):
                calls += 1
                if int(m.group(1), 16) not in usa:
                    fail(f"{src.name}:{n}: AfroGameAddr({m.group(1)}) is not in usa.toml")
            bare = rc.CALL.sub("", code)
            if ADDR.search(bare):
                fail(f"{src.name}:{n}: guest address outside AfroGameAddr(): {line.strip()}")
    if calls < 20:
        fail(f"only {calls} AfroGameAddr calls found; the scan is broken")

    header = rc.header("usa", usa)
    if "#define AFRO_REGION_USA 1" not in header or "case 0x828E629Cu: return 0x828E629Cu;" not in header:
        fail("USA header is not as expected")
    if "#define AFRO_REGION_USA 0" not in rc.header("eu", None):
        fail("Europe header is not as expected")
    if set(rc.XEX_SHA1.values()) != {"eu", "usa"}:
        fail("the two supported executables are not both recognised")
    print(f"region config: {len(usa)} USA addresses, {calls} AfroGameAddr calls, hooks and functions all mapped")


if __name__ == "__main__":
    main()
