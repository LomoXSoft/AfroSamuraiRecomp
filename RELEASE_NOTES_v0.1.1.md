# AfroSamuraiRecomp v0.1.1

Source-only release. The downloadable program is the setup installer. It builds
the game on your PC from your own Europe disc. No game executable and no game
files are included.

HDD installation can take a long time; SSD/NVMe recommended.

## Download

`afrosetup-v0.1.1.exe`

SHA-256 `3336ce0378a369f953476896bb091fb457f010fcacc1b5b77968c4a9e07da1c1`

1731355 bytes. This is the file that was installed and played. It is not rebuilt for this release.

## Highlights

- The setup program contains this 0.1.1 source. Install and Update do not clone public main.
- Original (30) stays the default. Experimental (60) stays opt-in and incomplete.
- Quad-list depth offset defaults to half. The Advanced setting for that offset is removed. An expert override can still set it.
- An ISO whose SHA-1 does not match the Europe pin is logged and the install continues. The pin is unchanged. A missing or unreadable ISO still stops the install.
- A failure to write `install.log` does not stop the install. The same lines go to `install-fallback.log` beside it, or to the temp file if that folder cannot be written. A real install failure is still shown and still stops the run.
- Resume keeps finished tools, verified downloads, extracted game data, and a completed SDK. The step that did not finish is not marked done.
- The patched GPU plugin links. LLVM extraction does not show a full bar before tar has a total. Git directory exceptions stay limited to the repos this installer creates.

## Tested

- Tested: a resumed HDD install finished, and the game session played correctly.
- Tested: a clean NVMe install ran all 14 steps, ended with installation OK, and the game session played correctly.
- Tested: the headband looked correct in play of this build.
- Those runs are not a promised duration and not a check of every scene.

## Still not claimed

- Invulnerability (experimental) does not prevent death. It stays off unless enabled for that launch, and it is not saved.
- Other black textures are not claimed fixed.
- Experimental (60) can still run cutscenes fast and drop frames.
- No install time is guaranteed on HDD, SSD, or NVMe.

## Headband comparison

![Headband: original GPU vs quad depth offset full vs half (default)](https://github.com/LomoXSoft/AfroSamuraiRecomp/releases/download/v0.1.1/headband-comparison-v0.1.1.png)

Development rendering comparison: original (left), full (middle), and half (right). Half is the default in 0.1.1.
