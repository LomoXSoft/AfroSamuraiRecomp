# Building the Windows game on Linux – Phase 1 feasibility

Goal: build the **Windows** game (`afrorecomp.exe`, `rexruntime.dll`, patched
`rexgpu-xenos.dll`) on Linux/SteamOS from the player's own disc, then run it
through Proton. No native Linux port.

Tested in WSL2 Ubuntu 26.04 on the test PC (Intel i7-10700F, 8 cores / 16
threads), with no root and no apt: every tool unpacked under `~/afro-linux`,
the way `install.sh` would have to work on SteamOS. Europe disc, v0.3.2 source.

## Result: feasible

| Step | Result |
| --- | --- |
| Codegen with the Linux `rexglue` | Works. Prebuilt Linux nightly of the pinned commit `bd833a2`. |
| Same code as Windows | All 52,676 functions are identical. Files are split differently (see below). |
| Cross-compile `afrorecomp.exe` | Works: clang 20.1.8 + lld-link, MSVC CRT and Windows SDK from `xwin`. |
| Cross-compile `rexruntime.dll`, `rexgpu-xenos.dll` | Work. SDK tree checked equal to `bd833a2` + all 14 patches, and both markers that `install.ps1` checks are present. |
| Imports and exports | Same DLL list as the Windows build for all three files. Every `rexruntime.dll` symbol the exe (432) and the GPU plugin (105) import is exported. |
| Runs | The Linux-built files, in a copy of the game folder on the test PC (Windows, not Proton), boot into the game. The log matches the Windows build line for line except thread IDs, at a steady 30 FPS in game. |
| Under Proton | **Not tested yet**: this needs the Steam Machine. |

## Findings that shape `install.sh`

1. **Use the prebuilt Linux `rexglue`.** The nightly
   `rexglue-sdk-0.10.0.24-dev.gbd833a2-linux-amd64.zip` (122 MB) has it. No
   Afro patch touches codegen (they change audio, GPU, input and kernel
   files only). Building `rexglue` from source needs C/C++ headers and
   `crt1.o`, which neither WSL nor SteamOS has without root. That test was
   skipped for the same reason.
2. **Codegen splits files differently from Windows, but the code is the same.**
   `rexglue` assigns functions to the `afrorecomp_recomp.N.cpp` files
   differently on Linux (most likely a different hash-map order). The function
   bodies are identical, and two Linux runs give byte-identical output. Only the
   file split differs. Linux builds and Windows builds are therefore not
   byte-comparable, which matters to nothing that ships.
3. **Use llvm-mingw 20250709 (LLVM 20.1.8) instead of the official LLVM tarball.**
   The official `LLVM-20.1.8-Linux-X64.tar.xz` is 2.0 GB, unpacks to 11 GB,
   and its `lld-link` needs `libxml2.so.2`. Ubuntu 26.04 has only
   `libxml2.so.16`, and current Arch, which SteamOS is based on, is likely
   the same, though not checked here. llvm-mingw is the same clang 20.1.8 in
   76 MB (553 MB unpacked). It needs only glibc 2.34, libstdc++, zlib and zstd.
   Its clang is used with `--target=x86_64-pc-windows-msvc`, so the MinGW
   part is not used.
4. **The Windows SDK package must point at the Linux `rexglue`.**
   `generated/rexglue.cmake` runs `$<TARGET_FILE:rex::rexglue>`, which is
   `rexglue.exe` in the win-amd64 package. One `sed` on the unpacked
   `rexglueTargets-*.cmake` points it at the Linux binary.
5. **Let Ninja run codegen; do not run it beforehand.** A codegen run before
   the first build is repeated by Ninja (no `.ninja_log` entry yet), which
   costs about 2 minutes.
6. **Toolchain details** (draft in `tools/linux/win-cross.cmake`):
   - `CMAKE_SYSTEM_NAME Windows`, clang/clang++ with the MSVC target, and LLD.
   - `llvm-ar` as the archiver: CMake calls it GNU-style, so `llvm-lib` fails.
   - `llvm-rc` as the resource compiler. There is no `llvm-mt`, and lld-link
     embeds the manifest itself.
   - Release try-compiles: `xwin` has no debug CRT.
   - Headers and libraries come through the `INCLUDE` and `LIB` environment
     variables, which clang and lld-link read the same way cl and link do.
7. **Case-sensitive file names.** The SDK sources include `<ObjBase.h>`, and
   some library names use mixed case. `tools/linux/xwin_case_links.py`
   (draft) scans the sources and adds 5 header and 9 library symlinks to the
   `xwin` tree.
8. **MSVC version.** `xwin` 0.10.0 gets CRT 14.44 and Windows SDK 10.0.26100.
   The Windows setup uses 14.51 and 10.0.28000. The only visible effect is a
   few different imports: `CreateFileW` and `CopyFileW` instead of
   `CreateFile2` and `CopyFile2`, plus the crash-report helpers of the older
   CRT (`_invoke_watson`, `RtlCaptureContext`, `RtlVirtualUnwind`,
   `UnhandledExceptionFilter`, `IsProcessorFeaturePresent`). All of them exist in Windows
   10 and 11 and in Wine.

## Risks to check on the Steam Machine

- **VC++ runtime under Proton.** All three files import `msvcp140.dll`,
  `vcruntime140.dll`, `vcruntime140_1.dll` and `msvcp140_atomic_wait.dll`.
  The Windows game folder ships none of them: Windows PCs have the VC++
  redistributable installed. A Proton prefix for a non-Steam shortcut does
  not, so Wine's own builtin versions are used. If the game fails at start, the
  fix is to put Microsoft's redist DLLs next to the exe. They come as zip
  (VSIX) packages in the same Visual Studio manifest that `xwin` reads, and
  the redist license allows app-local copies. That would be a small Python
  step.
- **libstdc++ version.** The prebuilt `rexglue` needs `GLIBCXX_3.4.32`
  (GCC 13.2 or later). Current SteamOS should have it. To check:
  `grep -c GLIBCXX_3.4.32 /usr/lib/libstdc++.so.6` (needs to print 1 or more).
- **Memory and heat on the Deck.** The 129 MB of generated C++ is compiled
  with `-j` set to the number of threads. If 16 GB is tight, use a lower `-j`.

## Time and disk on a Deck-class CPU

Measured with a clean build pinned to 8 logical CPUs (`taskset -c 0-7`) on
the i7-10700F:

| Step | Time |
| --- | --- |
| Codegen | 127 s |
| App compile (without the repeated codegen) | about 75 s |
| GPU plugin + runtime | 127 s |
| **Build total** | **about 330 s** |

The Deck's Zen 2 (4 cores / 8 threads, 2.4–3.5 GHz) is roughly 1.5–2× slower
per thread. **Estimate: 8–11 minutes of building**, plus downloads and the
ReXGlue clone (253 s here, mostly network). These are estimates, not Deck
measurements.

Downloads: llvm-mingw 76 MB, CMake 65 MB, Ninja 0.1 MB, xwin 3.4 MB, ReXGlue
nightly (Linux and Windows) 167 MB, pycryptodome 3 MB, plus the Microsoft
CRT/SDK packages that `xwin` fetches (1.1 GB cache, which can be deleted
after the splat leaves 631 MB) and the ReXGlue source (about 710 MB
blobless with submodules). The peak is about 4.5 GB beside the extracted
game files.

## Phase 2 results (`install.sh` in WSL, no root)

- A fresh install from the Europe ISO (path with spaces and brackets)
  downloads and checks every pinned file, applies all 14 patches, verifies
  and extracts the ISO, builds and sets up `game/`. Nothing is written
  outside the install folder. Build steps on 16 threads: game 169 s, GPU
  plugin and runtime 62 s.
- A failed run resumes with the same command; finished steps are skipped.
- `--update` with one changed source file: about 16 s. Only that file is
  recompiled, codegen is reported up to date, and `app/region`, the game files
  and the settings are kept (only the cfg `version=` line changes). With
  nothing changed, nothing is compiled.
- The `game/` folder it produced, run natively on the test PC, skips the
  launcher (`ask=0`, current version) and plays.
- Found while testing:
  - `app/generated/rexglue.cmake` is tracked and must be synced, unlike
    `app/generated/default`.
  - CMake must be configured again after the first codegen, because the
    recompiled sources are listed in `generated/default/sources.cmake`.
  - `install.sh` needs mode 100755 in git: Git on Windows does not record it
    by itself.

## Proposal for Phase 2 (`install.sh`)

Bash, no root, everything under the install folder:

1. Check for `python3`, `git`, `curl`, `tar`, `xz` and libstdc++ `GLIBCXX_3.4.32`,
   and stop with a clear message otherwise.
2. Download with pinned SHA-256: llvm-mingw, CMake, Ninja, xwin, both ReXGlue
   nightly zips and the pycryptodome wheel. Unzip with `python3 -m zipfile`
   (SteamOS has no `unzip`).
3. Run `xwin --accept-license splat`. The script asks first, like the Windows
   setup does for the MSVC license. Then add the case links and delete the
   xwin cache.
4. Clone ReXGlue at `bd833a2` and apply the patches, with stamps as in
   `build_gpu_plugin.bat`.
5. Input: an ISO or an extracted folder, with the SHA-1 checks from
   `install.ps1` (`xiso_extract.py`). Then `region_config.py`, `xex_to_pe.py`,
   `gen_switch_tables.py` and `extract_icon.py`.
6. Configure and build the app (Ninja runs codegen), then the GPU plugin and
   runtime. Verify the two patch markers.
7. Set up `game/` with the glyphs, `recomplauncher.json`, the icon and the
   launch options `--game_data_root="../gamedata" --gpu_plugin=xenos
   --log_file="../afro.log"`.
8. Steam shortcut: print the exact manual steps first. Writing
   `shortcuts.vdf` (binary, only while Steam is closed) can follow once the
   manual way works.
9. `--update`: keep `app/region`, the saves and the cfg, then rebuild
   incrementally.
