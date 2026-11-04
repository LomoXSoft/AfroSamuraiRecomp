# Changelog

Newest first. Versions are in `app/VERSION`.

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
