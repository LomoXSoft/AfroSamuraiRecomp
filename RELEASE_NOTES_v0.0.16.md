# AfroSamuraiRecomp v0.0.16 – Experimental 60 FPS

Source-only prerelease. **No binaries or game files.** Build from your own Europe ISO.

## Highlights
- **Original (30) remains the default** and is the recommended play mode.
- New opt-in **Experimental (60)** in Advanced settings for public testing and log collection.
- All previously released voice / black-screen / language / saves / installer work from 0.0.12–0.0.15 is kept.
- Safer update path: codegen refreshes when `afro_hooks.toml` changes.

## Changes
- Launcher FPS choices reduced to **Original (30)** and **Experimental (60)** only (removed misleading host-only caps and any unmapped tick-mode UI).
- Experimental sets `afro_present_interval=1` + host cap 60 and enables **gated** guest dt / 1/30→1/60 hooks. Original keeps present interval default **2** and skips those Experimental patches.
- Bounded ~2s FPS/dt/present-interval logging for testers (`afro_fps_log`, overwritable log file).
- Docs: README tester guide, truthful log paths (installer vs `run_win.bat`), GitHub issue template.
- Pre-0.0.16 remembered FPS values map back to Original (they were only a host cap on still-30 gameplay).

## Known issues
- **Experimental (60):** intro and cinematics can still be accelerated; other gameplay / Focus / combat timing may be wrong and is largely unverified. Not claimed fixed or “smooth 60”.
- FPS dips can still happen (including below 30, and dual-camera stretches near ~30). Cause is **not proven** without profiling — do not assert a single GPU root cause.
- Stuck double tutorial prompt (original game behaviour).
- (No muffled-audio issue is listed as confirmed.)

## Building
Use `install.bat` (fetches this revision from GitHub after publish) or a source checkout + `tools\build_gpu_plugin.bat` then `tools\build_win.bat`. See README. Requires your own Europe disc/ISO.

## Tested on
- **Confirmed earlier (prior releases / Original path):** boots, menus, gameplay, voices, RTV black-frame fix on one Windows PC (AMD Radeon RX 9060 XT) at Original-style 30 FPS pacing.
- **This candidate (0.0.16 rollback tree):** sources prepared from the post-crash diagnostic rollback (`e1f0a91` / 4c08751-class timing). A Windows **compile** of that rollback succeeded; **that build was not launched** in the rollback session. Static/gating audit only for this tag candidate — **not** a substitute for gameplay verification. Please smoke-test Original and a short Experimental session before relying on it.

## Legal
Fan project, not affiliated with Namco Bandai, GONZO, Microsoft, or rights holders. No game code/assets distributed. GPL-3.0 for this repo except `patches/rexglue/` (BSD-3). Own the original game.
