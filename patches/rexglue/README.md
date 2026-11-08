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

## zz-afro-renderdoc-guest-capture.patch (GPU plugin, `rexgpu-xenos.dll`)
Applied after `zz-afro-perf-log.patch`. Instrumentation only. It does not change
shading. RenderDoc's own capture ends on `Present`, and this presenter can
`Present` the same guest image again, so those captures are only
`DrawInstanced(4, 1)`. Guest draws stay in a deferred list until
`EndSubmission` replays them. If RenderDoc launched the process, creating
`afro_renderdoc_capture.request` next to `afrorecomp.exe` (or in the install
folder) captures the next guest frame: `StartFrameCapture` when that frame
opens, `EndFrameCapture` after its closing `ExecuteCommandLists`, before
`PaintAndPresent`. `--afro_renderdoc_capture=1` does the same once per process.
Default is off. Nothing is captured when RenderDoc is not attached.

## zzz-afro-quad-depth-offset.patch (GPU plugin, `rexgpu-xenos.dll`)
Same license (BSD 3-Clause). Applied last. Experimental, default `off`. Not a
console-correct fix and not a change to every draw.

`afro_quad_depth_offset` is `half` by default, or `full` or `off`. The launcher
does not show this choice. It is init-only: an explicit command-line value or
`REX_AFRO_QUAD_DEPTH_OFFSET` overrides the default for that process. `off` is
the original integer bias.

When the mode is `full` or `half`, a draw is changed only if all of these hold:
host render-target path (not ROV), not tessellated, guest primitive is a quad
list, quad lists are still expanded by the geometry shader, and the guest
constant polygon offset converts to a nonzero D3D integer bias. The rasterizer
constant `DepthBias` is then 0. `SlopeScaledDepthBias` stays. The quad geometry
shader adds `clip.z += C * clip.w`, with the same C on all four vertices.
`full` uses `bias/16777216` viewport-depth units (the exponent the integer was
sized for). `half` uses half of that. C is divided by the viewport depth range
of that draw. A zero or non-finite range uploads 0, so that draw loses the
constant as well. Zero-offset draws, including the depth-only passes that share
the shader but have bias 0, keep the original geometry shader and the original
bias. Depth-only draws that do have a nonzero constant are shifted too.

The pipeline description still stores the original integer, so it does not
collide with a real zero-bias pipeline. The geometry shader variant is chosen
when the pipeline is created. Changing the mode needs a new process.

The add happens before clipping. A vertex already within about |C| of the near
or far plane can cross it. This build does not claim that does not happen.

## zz-afro-perf-log.patch (GPU plugin, `rexgpu-xenos.dll`)
Same license (BSD 3-Clause, modifies ReXGlue code; code only). Diagnostics for
the FPS drops, all default off:
- `afro_perf_log` (false): per-second summary `afro perf 1s: ...` plus
  `afro perf slow frame N: ...` for frames longer than `afro_perf_slow_ms`
  (37): draws, copies, RT changes, EDRAM transfers, new pipelines/shaders,
  texture loads, uploads, readbacks and their GPU-sync stalls, occlusion
  queries, and time split into idle / waits / busy. Capped by `afro_perf_log_max`.
- `afro_occlusion_async` (false), experimental: occlusion queries no longer
  stall the CPU per query; results are written when the GPU submission finishes.

## zzzz-afro-button-glyphs.patch (GPU plugin and runtime)
Same license (BSD 3-Clause, modifies ReXGlue code; code only, no game art).
Applied after `zzz-afro-quad-depth-offset.patch`. Button prompts that follow
the controller.

- Runtime (`src/input/sdl/sdl_input_driver.cpp`): `afro_pad_family` records the
  family of the pad that last pressed a button or pulled a trigger (the first
  connected pad sets it before that): 1 Xbox, 2 PlayStation, 3 Nintendo,
  4 Steam (Valve vendor ID). It uses `SDL_GetGamepadType`, the type SDL meant
  for button labels.
- GPU plugin (`src/graphics/afro_glyphs.*`, texture caches): the game draws each
  prompt as its own DXT5 texture, 64x64 for A/B/X/Y and 64x128 for the
  triggers. When one loads, its base level is untiled and hashed (FNV-1a 64).
  Known hashes come from RenderDoc captures of the Europe disc. For a non-Xbox
  family, the loaded levels are replaced by `glyphs\<family>\<NAME>.png` next to
  the exe (fitted, centred, mipmapped and BC3-compressed when first used).
  Xbox keeps the game's icons. When the family changes, the prompt textures
  reload.
- `afro_button_glyphs` (`auto`): `auto` follows the last used pad, or `xbox`,
  `playstation`, `nintendo`, `steam`.
- `afro_glyph_log` (false): logs every small DXT5 texture with its hash and
  saves it as `glyph_dump\<w>x<h>_<hash>.dds` next to the exe, to find the
  prompts not identified yet.

Icon files are named by the Xbox button position the game asks for: on
Nintendo, `A.png` shows the B label that sits where Xbox A is. A missing file
keeps the game icon.

## Build
`tools\build_gpu_plugin.bat` applies every `patches\rexglue\*.patch` in name
order to `<root>\rexglue-sdk`, builds only `rexgpu-xenos.dll` and
`rexruntime.dll` and copies them next to the exe (the stock DLLs are kept as
`*.dll.stock`). Already-applied patches are detected even when later patches
touch the same lines (the whole stack is checked on a scratch git index, the
working tree is not touched) and remembered with stamp files in
`rexglue-sdk\.git\afro-patches`. `tools\build_win.bat` restores the patched
DLLs at the end, so the two scripts can be run in either order.
