# Changelog

Newest first. Versions are in `app/VERSION`.

## 0.1.0 – Fully playable
- Graphical setup program (`afrosetup.exe` on the release, not in git). It
  installs or updates, in English, Español, Français, and Italiano, without
  administrator rights. `install.bat` remains the console installer.
- Update keeps the extracted game, saves, and launcher settings, and rebuilds
  even when the folder is already 0.1.0.
- Launcher labels: `30 (original)`, `60 (experimental)`, `RTV (original)`,
  `ROV (experimental)`. Cheats are a separate dialog.
- Invulnerability (experimental) is off by default and is not saved. It does
  not currently prevent death.
- Known issues: FPS dips especially above native resolution, black textures
  including the headband, occasional odd audio, Experimental (60) can accelerate
  cutscenes and drop frames.
- The author's Windows PC: a clean install with the first 0.1.0 setup started
  the game, and a same-version update started it from Jugar. Not an independent
  full playthrough. The Cheats warning and the shorter progress line were not
  run again on Windows after they were added.

## 0.0.16 – Experimental (60) FPS opt-in (incomplete)
- Launcher Advanced settings: frame rate is now **Original (30)** (default) or
  **Experimental (60)** only. The old host-only 30/60 cap choices and any
  unmapped tick-mode selector are gone.
- Experimental (60) sets `afro_present_interval=1` and a host cap of 60, and
  enables gated guest dt / 1/30→1/60 const hooks. **Original leaves the default
  present interval (2) and does not apply those Experimental dt/const patches.**
- Bounded FPS/dt/present-interval log lines (~2s) to help testers; no game-data
  dumps. `afro_fps_log` still defaults on.
- `tools/build_win.bat` regenerates ReXGlue codegen when `afro_hooks.toml`
  changes (needed for mid-asm hooks on update/rebuild).
- Installer / portable source builds pick this up when the GitHub revision
  updates (re-run `install.bat` / rebuild).
- **Not claimed fixed:** intro and cutscenes can still run too fast at 60;
  other gameplay / Focus / combat timing is unverified. Prefer Original (30)
  unless you are testing. See README Known issues and the tester guide.
- Pre-0.0.16 remembered FPS values are treated as Original (they were only a
  host cap on still-30 gameplay). Choose Experimental (60) explicitly after
  updating.

## 0.0.15 – one-click installer
- New `install.bat` (+ `tools/install.ps1`): language first (en/es/fr/it), all
  questions up front, then unattended with total % progress. Portable toolchain
  (no admin, PATH or registry changes), SHA-256-pinned downloads, ISO SHA-1
  check, fast tar extraction, light ReXGlue clone, resumable, refreshes the
  source and rebuilds on re-run, self-contained `game\` folder + shortcuts,
  optional cleanup of build files (~8 GB).
- Launcher: "Manage saved games..." (open saves folder, back up, restore,
  open backups folder, delete; safety backups before restore/delete).
- Launcher: closing the window (X/Esc/Alt+F4) no longer starts the game;
  layout gap fixed.
- README rewritten: simple Quick start for new players, technical content
  under Advanced.

## 0.0.14 – all cutscene voices fixed, experimental advanced settings
- Fixed: cutscene lines silent after a camera cut, and everything after them
  (e.g. the second boss cutscene). Cause: when a stream's next packet holds no
  frame start, the decoder jumped to packet 0 of the next input block (another
  stream). New runtime patch `patches/rexglue/afro-xma-frameless-chain.patch`
  (cvar `afro_xma_frameless_chain`, default true).
- Launcher: new "Advanced settings (experimental)" dialog with the FPS cap
  (default "Original (30 capped)" = no override) and a sepia filter toggle.
- Launcher: internal resolution defaults to native (1x); resolution, FPS and
  sepia are only kept with "remember".
- New diagnostics patch `patches/rexglue/zz-afro-perf-log.patch`: `afro_perf_log`
  and experimental `afro_occlusion_async` (both off).
- Build scripts: `build_gpu_plugin.bat` detects an already-applied patch stack
  (and keeps stamps); `build_win.bat` restores the patched DLLs.
- Known issues: FPS drops when objects break or the camera splits;
  if two tutorial prompts appear at once, the first one can stay
  stuck on screen (original game behaviour; restarting the level clears it).

## 0.0.13 – cinematic voices fixed
- Fixed: cinematic dialogue lines silent or cut off (e.g. at camera cuts).
  Cause: these lines are multi-stream (5.1) XMA with the voice in the centre
  stream; the XMA decoder restarted at packet 0 on every new 32 KB input block,
  so the centre stream was lost. New runtime patch
  `patches/rexglue/afro-xma-fix-log.patch` (cvar `afro_xma_substream`, default true).
- XMA decoder: backport of Xenia-canary's `Work()` loop / output-valid handling
  (cvar `afro_xma_fix`, default true); optional `afro_xma_log` diagnostics.
- `tools/build_gpu_plugin.bat` now also builds and installs `rexruntime.dll`
  (stock kept as `rexruntime.dll.stock`).
- Known issue: some tutorial pop-ups advance too fast.

## 0.0.12 – RTV black frames fixed, stable 30 FPS
- Fixed: black frames in combat and at cutscene camera cuts on the RTV path.
  Cause: EDRAM range wraparound in the render target cache. The end-of-frame
  depth-only pass (at tile 736, estimated 2048 tiles long) wrapped round to tile 0
  and took ownership of the colour image (tiles 0-720); the lossy
  colour->depth->colour transfer blacked out frames whenever the game skipped the
  pass that restores it. New GPU plugin patch `patches/rexglue/afro-edram-no-wrap.patch`
  (cvar `afro_edram_no_wrap`, default true) clamps the range instead of wrapping.
- Default GPU render path is RTV again: correct image at a stable 30 FPS.
  ROV remains selectable as a fallback (Advanced options / `--advanced`, or
  `--render_target_path_d3d12=rov`).
- `tools/build_gpu_plugin.bat` now applies every `patches/rexglue/*.patch` in
  name order, skipping patches that are already applied.
- Known issue: some gameplay voice lines are silent or cut off.

## 0.0.11 – first public preview (source only)
- Default GPU render path is ROV (`render_target_path_d3d12=rov`): no black frames,
  but slow on some GPUs. The faster RTV path still shows black frames in combat and
  at cutscene camera cuts (under investigation).
- Launcher (EN/ES/FR/IT): language, frame rate cap, fullscreen/window, internal
  resolution 1x/2x/3x, "remember" option. Advanced (diagnostic) options are hidden;
  start with `--advanced` to show them. Saved advanced options are ignored without it.
- Fixed: on/off advanced rows whose default is off had no "On" choice.
- Diagnostics off by default (`afro_fx_log`, `afro_int_fetch_log`, `afro_rt_log`).
- New `tools/xex_to_pe.py` and self-contained `build_win.bat` (extracts PE,
  switch tables and icon from your own XEX on first build).
- GPU plugin patch: xenia-canary d119505 part A + render-target diagnostic log.

## 0.0.1 – 0.0.10 (private development)
- ReXGlue codegen project, manual function boundaries and jump-table overrides,
  crash fixes (unregistered code pointers, truncated switch tables), language
  picker and launcher, frame pacing hook, game fullscreen-effect toggles,
  launcher cvars applied to the GPU plugin, d119505 backport, diagnostics.
