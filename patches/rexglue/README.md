# ReXGlue GPU plugin and runtime patches

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

## afro-xma-fix-log.patch (runtime: XMA audio decoder, `rexruntime.dll`)
Modifies BSD-3 ReXGlue code (Copyright (c) 2026 Tom Clay; portions Copyright
(c) 2022 Ben Vanik and Xenia project contributors) and is distributed under the
same BSD 3-Clause license. It contains code only, no game data.
- cvar `afro_xma_substream` (default true), our own fix: cinematic dialogue is
  stored as multi-stream (5.1) XMA, three interleaved stereo streams with the
  voice in the centre stream. The decoder restarted at packet 0 of every new
  input block, so streams 1-2 decoded stream 0's packets and the centre channel
  was lost (lines silent or cut off at camera cuts). The fix continues each
  stream at its next packet in the next block.
- cvar `afro_xma_fix` (default true): backport of Xenia-canary's
  `XmaContext::Work()` loop and `output_buffer_valid` handling (only invalidate
  the output buffer when it is really full, drain the current frame, stop on no
  progress). Credit: Xenia-canary project contributors.
- cvars `afro_xma_log`, `afro_xma_log_all`, `afro_xma_log_max` (default off):
  diagnostic log of XMA context state.
Set `--afro_xma_substream=false` / `--afro_xma_fix=false` for A/B tests.

## afro-xma-frameless-chain.patch (runtime: XMA audio decoder, `rexruntime.dll`)
Same license as above (BSD 3-Clause, modifies ReXGlue code; code only).
- cvar `afro_xma_frameless_chain` (default true): when a stream's next packet
  holds no frame start (the frame spans the whole packet), keep following that
  stream's own packet chain (skip counts), also into the next input block,
  instead of jumping to packet 0 of the next block, which belongs to another
  stream. Fixes cutscene lines that went silent after a camera cut and stayed
  silent (e.g. the second boss cutscene).
- `--afro_xma_log=true` also logs `afro xma ctx N chain ...` lines.

## zz-afro-perf-log.patch (GPU plugin, `rexgpu-xenos.dll`)
Same license (BSD 3-Clause, modifies ReXGlue code; code only). Applied last
(name order). Diagnostics for the FPS drops, all default off:
- `afro_perf_log` (false): per-second summary `afro perf 1s: ...` plus
  `afro perf slow frame N: ...` for frames longer than `afro_perf_slow_ms`
  (37): draws, copies, RT changes, EDRAM transfers, new pipelines/shaders,
  texture loads, uploads, readbacks and their GPU-sync stalls, occlusion
  queries, and time split into idle / waits / busy. Capped by `afro_perf_log_max`.
- `afro_occlusion_async` (false), experimental: occlusion queries no longer
  stall the CPU per query; results are written when the GPU submission finishes.

## Build
`tools\build_gpu_plugin.bat` applies every `patches\rexglue\*.patch` in name
order to `<root>\rexglue-sdk`, builds only `rexgpu-xenos.dll` and
`rexruntime.dll` and copies them next to the exe (the stock DLLs are kept as
`*.dll.stock`). Already-applied patches are detected even when later patches
touch the same lines (the whole stack is checked on a scratch git index, the
working tree is not touched) and remembered with stamp files in
`rexglue-sdk\.git\afro-patches`. `tools\build_win.bat` restores the patched
DLLs at the end, so the two scripts can be run in either order.
