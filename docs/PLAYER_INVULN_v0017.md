# Player invulnerability — experimental in 0.1.0

Release 0.1.0 does **not** claim this works. With the char-HP hooks on, a
Windows session still ended in death (`clamps=717`, `restores=0`). The Cheats
checkbox is labeled experimental and warns that it does not prevent death.
Default remains off, and the choice is not saved.

# Player invulnerability — correction attempted for 0.1.0

The 0.0.17 char-HP hooks were confirmed on a Windows build: codegen fired,
`clamps=717`, `restores=0`, and the log showed `player HP store 90.0 -> 100.0`.
The player still died. Char `+0x280` was held. Death is a later predicate.

`sub_82666218` keeps the original value in `f30` and, after the char stores,
does `lwz r3,2004(r31)`, `fmr f1,f30`, `bl 0x82663180` (the `bl` is at
**`0x82666314`**, because the generated link register is `0x82666318`).
`sub_82663180` loads **`meter+0` into `f0`**, compares it with 0, and
`ble` to **`loc_8266320C`** when `meter+0 <= 0`. That block clears meter+60,
sets meter+63, and calls the fail helpers. **`f1` is not that compare.**
Changing only `f1` does not stop the branch.

This build keeps the char hooks and adds two before-instruction hooks that
write **player `meter+0`** only:

| Guest PC | Symbol | Registers |
|----------|--------|-----------|
| `0x82666314` | `AfroPlayerMeterBeforeDeathCall` | `r31` entity, `r3` meter |
| `0x82663180` | `AfroPlayerMeterDeathGate` | `r3` meter (entry, before `lfs`/`fcmpu`/`ble`) |

Both return immediately when `afro_player_invuln` is off. The call-site hook
also returns when `r31` is not the player singleton. The entry hook writes
only when `r3` equals `*(player+0x7D4)`. The value is the char-HP hold capped
at `player+0x284`. `0x82663264` (`AfroDt30_f1`) is left unchanged.

`meter_guards` on the ~2s fps line counts those writes. This correction has
**not** been run in the game.

# Player invulnerability (testing) — history from v0.0.17

Session-only optional cheat for campaign/boss testing. **Default OFF.**
Local branch name at handoff: `v0.0.17-headband-blackbands`. Not a public release.
This correction was not run against the game. No runtime result is claimed.

## Goal

Block **protagonist** health loss only. Prefer preserving hit reactions / mission
triggers. Do not touch saves. Do not make enemies invulnerable.

## Verified layout

| Item | Evidence |
|------|----------|
| Player singleton | `lwz` from **`0x828E629C`** |
| HP meter | `player + 0x7D4` → meter object |
| Char HP float | `player + 0x280` (codegen offset **640**) |
| Max HP float | `player + 0x284` (codegen offset **644**). Ceiling for restores. This code never writes it |
| Alt mirror | `player + 0x288` (codegen offset **648**). Not used as the restore target |
| Meter current | `meter + 0x4`. Secondary. Not the restore target |
| Lethal-block flag | `meter + 0x3C`. Secondary retail gate. Not sufficient |

`invulnerableTime` (`0x82036FC4`) sits with collectible props. Not the player.

## Combat HP writer

Guest function **`sub_82666218`**:

- Prologue saves LR / r31 / f30 / f31, then `fmr f30,f1` and `mr r31,r3`.
- Two stores of the value about to become char HP, both `stfs f0,640(r31)`:
  - **`0x826662D8`**
  - **`0x82666308`**
- Each is followed by `lwz r3,2004(r31)`. 2004 = **`0x7D4`** (the meter pointer).
- 640 = **`0x280`**.

The PowerPC word for `stfs f0,640(r31)` is **`D01F0280`**.

**Not the writer:** `sub_82674030` / VA `0x826740D8`. That was a codegen label
collision (wrong function body). Nothing here hooks it.

### Codegen vs attached PE text

The codegen listing of `sub_82666218` shows those two `stfs` instructions at
those PCs. Mid-asm hooks bind to those recompiled guest PCs.

The attached `pe_sub_82666218_disasm.txt` does **not** agree. At the labeled
addresses it shows `826662D8: 2B050000` and `82666308: 7C0C002E`, and the
labeled entry `82666218` is `lwz r10,72(r11)` rather than `mflr r12`. That text
dump is not confirmation of the store sites. After codegen, confirm the
generated function calls `AfroPlayerHpStoreHook` immediately before both
`REX_STORE_U32(ctx.r31.u32 + 640, ...)`. On a local `default.pe`, the guest
word at those VAs should be `D0 1F 02 80` if the image matches the codegen.

## Implementation

- CVar: `afro_player_invuln` (bool, **default false**).
- Launcher: Advanced settings → **Invulnerability (testing)** / **Invulnerabilidad (pruebas)**. **Not written to `afro.cfg`.** Each launch starts OFF unless the control is set again or the cvar is passed explicitly.
- RTV/ROV launcher path from the unpublished .17 sources is unchanged.

### Mid-asm (the combat path)

`app/afro_hooks.toml` registers `AfroPlayerHpStoreHook` at `0x826662D8` and
`0x82666308` with `registers = ["r31", "f0"]`. `after_instruction` is omitted, so
ReXGlue's default is **before** the `stfs`. The host function is:

```cpp
void AfroPlayerHpStoreHook(PPCRegister& r31, PPCRegister& f0);
```

That matches the existing mid-asm convention (`PPCRegister&` in toml order,
void, no `return` / `jump_address` keys). `f0.f64` is the source float the
following `stfs` consumes.

When the cvar is **off**, the hook returns without reading or writing guest
HP. When it is **on**:

1. Load the player singleton from `0x828E629C`.
2. If `r31` is not that pointer, return. Enemy and other-entity stores keep
   the guest `f0`.
3. If `f0` would lower char HP, or would store a value above a trusted
   `player+0x284`, replace `f0` with the session hold capped at that max.

The hold is the high-water mark of **char HP** (`+0x280`) while the cvar is
on. It is cleared when the cvar is off. The meter current is never copied
into the hold. There is no baked full-HP constant (the erroneous **110** came
from treating a stuck meter reading as the target and then writing that into
char HP and into `+0x284`).

A max field is trusted as a ceiling only when it is finite and at least 10,
and it either already covers the hold or exceeds it only by the modest
overshoot seen in the field log (within +20 and within 1.25×). A max that
contradicts a much higher live HP is ignored so a garbage max cannot collapse
the player.

### Fallback

`PlayerInvulnTick` still runs from `AfroSwapHook` and
`AfroPresentIntervalConsumeHook`. It may write **only** `player+0x280`, and
only to the same capped hold. It does not write `meter+0x4`, `player+0x284`,
or `player+0x288`. It still sets `meter+0x3C = 1` on the player meter as a
secondary retail flag. The field log showed that flag, plus post-sim rewrites,
do not stop `sub_82666218`.

### Diagnostics (bounded)

- At most 8 detail lines per process, then one suppression line.
- While the cvar is on (or after any clamp/restore), the existing ~2s fps line
  gains `invuln=on|off clamps=N restores=N`.
- `clamps` counts mid-asm replacements of player `f0`. `restores` counts
  fallback writes of `player+0x280`.
- Enemy stores are not logged.

If the fps line shows `invuln=on clamps=0` while the player is taking hits,
the generated image is not calling the hook (codegen was not regenerated, or
the store PCs do not match that `default.xex`).

## What this does not claim

- No playtest was run for this correction.
- Scripted deaths, pits, `Kill Player`, and cutscene fail-states can ignore
  `player+0x280`.
- Focus / other meters (`player+0x7D0`) are not clamped.
- Hit reactions can still play.
- A max-HP change the guest writes to `+0x284` becomes the new ceiling only
  when that value still agrees with the hold, as above.

## Codegen required

`AfroPlayerHpStoreHook` does nothing until `rexglue codegen` rebuilds
`sub_82666218` with the two call sites. `tools/build_win.bat` reruns codegen
when `app/afro_hooks.toml` SHA1 differs from `app/generated/default/hooks.sha1`.
See `APPLY.txt` in the artifact. Do not ship this as a release.

## Field test 2026-11-06 (before this correction)

Launcher passed `afro_player_invuln`. `AfroSwapHook` logged
`afro invuln: ENABLED`. The player still died.

Log shape: `hp_meter=75 hp_char=100 hp_max=100`, then hundreds of
`restored HP` lines with the meter stuck near **110** while char HP stepped
100→90→…→0. Post-sim writes and `meter+0x3C` did not stop the store at
`player+0x280`. The restore target followed the meter, which is why 110 was
written back.

## Not done

- Rebuild or retest on the Windows PC.
- Push / public release.
