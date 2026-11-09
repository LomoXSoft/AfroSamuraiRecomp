#!/usr/bin/env python3
"""Static checks for the player-only invuln correction. No game, no codegen."""

from __future__ import annotations

import pathlib
import sys
import tomllib

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent if HERE.name == "checks" else HERE


def fail(msg: str) -> None:
    print(f"FAIL: {msg}", file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    toml_path = ROOT / "app" / "afro_hooks.toml"
    cpp_path = ROOT / "app" / "src" / "frame_pacing.cpp"
    hdr_path = ROOT / "app" / "src" / "player_hp_invuln.h"
    picker_path = ROOT / "app" / "src" / "language_picker.cpp"
    version_path = ROOT / "app" / "VERSION"
    doc_path = ROOT / "docs" / "PLAYER_INVULN_v0017.md"

    toml = tomllib.loads(toml_path.read_text(encoding="utf-8"))
    hooks = toml.get("midasm_hook")
    if not isinstance(hooks, list) or not hooks:
        fail("afro_hooks.toml has no [[midasm_hook]] entries")

    wanted = {0x826662D8: None, 0x82666308: None}
    banned = {0x82674030, 0x826740D8}
    names = {}
    for hook in hooks:
        addr = hook.get("address")
        name = hook.get("name")
        if addr in banned or name == "sub_82674030":
            fail(f"banned HP-writer address/name present: {hook}")
        names.setdefault(name, 0)
        names[name] += 1
        if addr not in wanted:
            continue
        if name != "AfroPlayerHpStoreHook":
            fail(f"{addr:#x} name is {name}")
        regs = hook.get("registers")
        if regs != ["r31", "f0"]:
            fail(f"{addr:#x} registers {regs}")
        if hook.get("after_instruction") not in (None, False):
            fail(f"{addr:#x} after_instruction must be omitted or false")
        for key in (
            "return",
            "jump_address",
            "return_on_true",
            "return_on_false",
            "jump_address_on_true",
            "jump_address_on_false",
        ):
            if hook.get(key) not in (None, False, 0):
                fail(f"{addr:#x} sets control-flow key {key}")
        wanted[addr] = hook
    missing = [f"{addr:#x}" for addr, hook in wanted.items() if hook is None]
    if missing:
        fail("missing HP store hooks: " + ", ".join(missing))

    # Reuse of one host symbol at several sites is already the file's pattern.
    if names.get("AfroDt30_f0", 0) < 2:
        fail("expected existing multi-site hook AfroDt30_f0 (convention check)")

    cpp = cpp_path.read_text(encoding="utf-8")
    hdr = hdr_path.read_text(encoding="utf-8")
    picker = picker_path.read_text(encoding="utf-8")
    version = version_path.read_text(encoding="utf-8").strip()
    doc = doc_path.read_text(encoding="utf-8")

    if version != "0.2.0":
        fail(f"VERSION is {version!r}")
    if "REXCVAR_DEFINE_BOOL(afro_player_invuln, false" not in cpp:
        fail("cvar default is not false")
    sig = "void AfroPlayerHpStoreHook(PPCRegister& r31, PPCRegister& f0)"
    if cpp.count(sig) != 1:
        fail("hook signature count != 1")

    ns_close = cpp.find("}  // namespace\n")
    hook_at = cpp.find(sig)
    afro_ns = cpp.find("namespace afro {", hook_at)
    if not (0 <= ns_close < hook_at < afro_ns):
        fail("AfroPlayerHpStoreHook is not a global symbol between the anon namespace and namespace afro")

    body = cpp[hook_at:cpp.find("\nvoid ", hook_at + 1)]
    cvar_at = body.find("REXCVAR_GET(afro_player_invuln)")
    cmp_at = body.find("r31.u32 != player")
    decide_at = body.find("DecideStore")
    if not (0 <= cvar_at < cmp_at < decide_at):
        fail("hook must test the cvar, then the player pointer, before DecideStore")
    if "return" not in body[:cvar_at + 80]:
        fail("cvar-off path must return")
    if "f0.f64" not in body[decide_at:]:
        fail("replacement must assign f0.f64")

    # Invuln must not write max, the alt mirror, or the meter current.
    for needle in (
        "kPlayerHpAltOff",
        "hold_meter",
        "WriteGuestFloat(player + kPlayerHpMaxOff",
        "WriteGuestFloat(meter + kMeterCurOff",
    ):
        if needle in cpp:
            fail(f"unexpected invuln write/input still present: {needle}")
    if cpp.count("WriteGuestFloat(player + kPlayerHpFloatOff") != 1:
        fail("fallback must write char HP exactly once")

    if "110" in hdr or "110" in body:
        fail("HP policy or hook body contains a literal 110")

    # Launcher: session flag is applied, never stored.
    if 'SetCvar("afro_player_invuln", "true")' not in picker:
        fail("launcher does not pass afro_player_invuln")
    save_at = picker.find("std::ofstream out(path")
    save_end = picker.find("#else", save_at)
    if save_at < 0 or save_end < 0:
        fail("launcher save block not found")
    save = picker[save_at:save_end]
    if "invuln" in save:
        fail("launcher save block mentions invuln")
    if "RTV" not in picker or "ROV" not in picker:
        fail("RTV/ROV launcher strings missing")

    for text, label in ((doc, "doc"), (cpp, "cpp")):
        if "0x826662D8" not in text or "0x82666308" not in text:
            fail(f"{label} missing store PCs")
        if "sub_82666218" not in text:
            fail(f"{label} missing sub_82666218")

    # stfs f0,640(r31) encoding, and the field-note offsets.
    word = (52 << 26) | (0 << 21) | (31 << 16) | 640
    if word != 0xD01F0280:
        fail(f"stfs encoding {word:#x}")
    if 640 != 0x280 or 644 != 0x284 or 2004 != 0x7D4:
        fail("offset identity check failed")

    meter_hooks = {
        0x82666314: ("AfroPlayerMeterBeforeDeathCall", ["r31", "r3"]),
        0x82663180: ("AfroPlayerMeterDeathGate", ["r3"]),
    }
    for hook in hooks:
        addr = hook.get("address")
        if addr == 0x82663264:
            if hook.get("name") != "AfroDt30_f1" or hook.get("registers") != ["f1"]:
                fail("0x82663264 must stay AfroDt30_f1")
        if addr not in meter_hooks:
            continue
        name, regs = meter_hooks[addr]
        if hook.get("name") != name or hook.get("registers") != regs:
            fail(f"meter hook {addr:#x} is {hook.get('name')} {hook.get('registers')}")
        if hook.get("after_instruction") not in (None, False):
            fail(f"meter hook {addr:#x} must run before the instruction")
        if "f1" in hook.get("registers", []):
            fail(f"meter hook {addr:#x} must not take f1")
        meter_hooks[addr] = None
    missing_meter = [f"{addr:#x}" for addr, hook in meter_hooks.items() if hook is not None]
    if missing_meter:
        fail("missing meter hooks: " + ", ".join(missing_meter))

    for name in ("AfroPlayerMeterBeforeDeathCall", "AfroPlayerMeterDeathGate"):
        at = cpp.find(f"void {name}(")
        if at < 0:
            fail(f"missing {name}")
        body = cpp[at:cpp.find("\nvoid ", at + 1)]
        if "REXCVAR_GET(afro_player_invuln)" not in body or "return" not in body:
            fail(f"{name} must return when the cvar is off")
        if "f1.f64" in body or "PPCRegister& f1" in body:
            fail(f"{name} alters or receives f1")
        if "GuardPlayerMeter" not in body:
            fail(f"{name} must guard meter+0")
    if cpp.count("WriteGuestFloat(meter + kMeterHp0") != 1:
        fail("meter+0 must be written in one helper")
    if "r31.u32 != player" not in cpp[cpp.find("AfroPlayerMeterBeforeDeathCall"):]:
        fail("call-site hook must ignore non-player entities")
    for token in ("hpP=", "hpO=", "callP=", "callO=", "gateM=", "gateX=", "lethal="):
        if token not in cpp:
            fail(f"fps probe missing {token}")
    if "g_inv_hp_other" not in cpp or "WriteGuestFloat" in cpp[cpp.find("g_inv_hp_other"):cpp.find("g_inv_hp_other")+400]:
        fail("other-entity hp stores must be counted, not written")

    # 0.2.0: Invulnerability is a checkbox on the Gameplay tab of Settings;
    # the frame rate (30 / 60) is on the Graphics tab, no longer experimental.
    if "L\"Experimental\"" not in picker:
        fail("English Experimental tab missing")
    cheat_btn = [ln for ln in picker.splitlines() if "kIdCheatInv" in ln and "BUTTON" in ln]
    if len(cheat_btn) != 1 or "BS_AUTOCHECKBOX" not in cheat_btn[0]:
        fail("invulnerability checkbox missing")
    if "kIdXInvuln" in picker:
        fail("inline invulnerability combo id still present")
    if "30 (original, default)" not in picker or "60 (experimental)" in picker:
        fail("frame-rate labels were not normalized")
    if "RTV (default)" not in picker:
        fail("RTV label was not normalized")
    settings = picker[picker.find("LRESULT CALLBACK SettingsProc"):picker.find("LRESULT CALLBACK PickProc")]
    if not settings or "st->play" in settings:
        fail("Settings window must not start the game")

    print("static invuln checks: ok")
    print(f"  hooks: AfroPlayerHpStoreHook @ 0x826662D8 and 0x82666308")
    print(f"  registers: r31, f0; after_instruction omitted")
    print(f"  VERSION {version}; cvar default false; no literal 110 in the policy")
    print(f"  stfs f0,640(r31) word D01F0280; 640=0x280; 2004=0x7D4")


if __name__ == "__main__":
    main()
