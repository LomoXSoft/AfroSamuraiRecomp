# ReXGlue GPU plugin patches

## xenia-d119505.patch (backport of xenia-canary d119505, part A)

Base: ReXGlue `development` @ `bd833a2a` (same commit as the SDK nightly
0.10.0.24-dev.gbd833a2). Upstream: xenia-canary commit d119505, "integer
num_format fetches on fixed-point textures".

License: these patches modify ReXGlue / Xenia code and is distributed under
their BSD 3-Clause license (Copyright (c) 2026 Tom Clay; portions Copyright
(c) 2022 Ben Vanik and Xenia project contributors). See the ReXGlue
repository's LICENSE. The rest of this repository is GPL-3.0.

## What it does (D3D12 GPU plugin only)
- Integer-fetch scaling for fixed-point textures (`TextureCache::GetIntegerScaleBits`,
  per-binding scale bits, system constant `texture_integer_scale_bits[32]`
  appended at the end of the cbuffer, DXBC fetch code). Format component widths
  are a table local to the plugin so the stock `rexruntime.dll` keeps working.
- cvar `afro_int_fetch_scale` (default true): the fix, can be turned off for A/B tests.
- cvar `afro_int_fetch_log` (default false): logs each affected texture format once.
- cvar `afro_rt_log` (default false) / `afro_rt_log_max`: diagnostic log of host
  render-target sets, EDRAM ownership transfers and resolves.

Not ported: part B (gamma PWL in resolve shaders; needs shader sources + FXC)
and the Vulkan path.

## afro-edram-no-wrap.patch (our own, not upstream)
Written for this project (independent of `xenia-d119505.patch`); it modifies
BSD-3 ReXGlue code and is distributed under the same BSD 3-Clause license.
Adds cvar `afro_edram_no_wrap` (default true): on the host render target path
(RTV) a render target's estimated EDRAM range is clamped at tile 2048 instead
of wrapping round to tile 0. Without it the end-of-frame depth-only pass
(`[d0]` at tile 736, estimated 2048 tiles) takes ownership of the colour image
at tiles 0-720, and the lossy colour->depth->colour transfer blacks out the
frame when the game skips the pass that restores it (camera cuts, combat).
ROV is not affected. Logs once: `afro edram no-wrap: clamped ...`.
`--afro_edram_no_wrap=false` disables it for A/B tests.

## Build
`tools\build_gpu_plugin.bat` applies every `patches\rexglue\*.patch` in name
order (skipping those already applied) to `<root>\rexglue-sdk`, builds
only `rexgpu-xenos.dll` and copies it next to the exe (the stock DLL is kept as
`rexgpu-xenos.dll.stock`). Re-run it after `build_win.bat`.
