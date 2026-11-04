# Changelog

Newest first. Versions are in `app/VERSION`.

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
