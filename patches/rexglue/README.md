# ReXGlue GPU plugin patch (backport of xenia-canary d119505, part A)

Base: ReXGlue `development` @ `bd833a2a` (same commit as the SDK nightly
0.10.0.24-dev.gbd833a2). Upstream: xenia-canary commit d119505, "integer
num_format fetches on fixed-point textures".

License: this patch modifies ReXGlue / Xenia code and is distributed under
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

`tools\build_gpu_plugin.bat` applies the patch to `<root>\rexglue-sdk`, builds
only `rexgpu-xenos.dll` and copies it next to the exe (the stock DLL is kept as
`rexgpu-xenos.dll.stock`). Re-run it after `build_win.bat`.
