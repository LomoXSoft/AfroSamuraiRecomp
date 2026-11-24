# AfroSamuraiRecomp v0.3.10 – Setup without embedded files

Source-only release. The downloadable program is the setup installer. It builds
the game on your PC from your own Europe or USA disc. No game executable and no
game files are included.

HDD installation can take a long time; SSD/NVMe recommended.

## Download

Windows: `afrosetup-v0.3.10.exe`

SHA-256 `bec0d5f2135142783d4de4e07a979049e95237fc9d07f6c85033e5a9c0e5744b`

412672 bytes. VirusTotal at release: 2 of 70 engines, both machine-learning
guesses (Microsoft `Trojan:Win32/Wacatac.B!ml`, Trapmine
`Malicious.moderate.ml.score`); reported to Microsoft as a false positive.
v0.3.9 had 4 of 70 at its last scan.

The setup downloads `afrosetup-v0.3.10-files.bin` from this release by itself
when you press **Install** (SHA-256
`40184892a51fdeca8e08550896546eddb1f1dc766abf1214e6d3130f1b8d0790`, 1753116
bytes). You only need to download that file yourself if the PC has no internet
at that moment: put it next to the setup.

Already on 0.1.x, 0.2.x or 0.3.x: run it, choose **Update** and pick your install
folder. Your extracted game, saves and launcher settings stay.

Linux and SteamOS (Steam Deck, Steam Machine): `AfroSamuraiRecomp-Linux-v0.3.10.desktop`,
see [Linux and SteamOS](#linux-and-steamos-proton) at the bottom.

## New

### The setup no longer carries the source files
- Up to v0.3.9 the setup exe held the project's source files (install.ps1, the
  build scripts, the game configuration) packed inside it, and started
  PowerShell on them after unpacking. Several antivirus engines read that as a
  "shell loader" (v0.3.9: Huorong, Rising and AliCloud `ShellLoader`/`Strab`,
  SecureAge), whether the files were appended to the exe (up to v0.3.7) or
  stored as a resource (v0.3.8, v0.3.9).
- Now the setup is a small exe (about 400 KB instead of 2 MB) and the source
  files are a second file on the release, `afrosetup-v0.3.10-files.bin`. When
  you press **Install** or **Update**, the setup uses that file if it lies next
  to the setup, or downloads it from this release. It accepts it only when its
  size and SHA-256 equal the ones built into the setup. A wrong or missing file
  stops before anything is written to the install folder, and the window goes
  back to the summary with the reason.
- The game itself is unchanged from v0.3.9.

## Tested

- Europe, fresh install of this release through the setup's own window, from
  the ISO, with the `.bin` next to the setup: 12.5 minutes. The game showed
  `afrorecomp_v0.3.10` and the intro movie played. Saves were unchanged.
- Setup without the `.bin` next to it, before this release existed: after
  **Install** it tried GitHub, showed "could not be downloaded (HTTP 404)" with
  the offline hint, went back to the summary, and created no install folder.
- Setup with a damaged `.bin` next to it (one byte changed): it was not used;
  the setup tried GitHub instead, with the same result.
- The download code against GitHub itself: it downloaded the v0.3.9 setup
  through GitHub's redirect and got its published SHA-256; a missing asset gave
  HTTP 404, an unknown host and a too-large file were refused.
- The checks in `checks/` pass, including the packaging check on the built
  files: no data resource, no overlay and no install.ps1 text in the exe, and
  the exe holds the `.bin`'s SHA-256.

## Still not claimed

- Antivirus results can change after release, and some engines may flag the
  setup.
- The setup is not signed.
- Closing the window during the download was not tested.
- RecompLauncher was not run with this release.
- No install time is guaranteed on HDD, SSD, or NVMe.

## Linux and SteamOS (Proton)

The Linux install builds the same **Windows** game from your own disc, and Steam
runs it through Proton. About 20 GB must be free. Tested on a Steam Machine
(v0.3.7); not tested on a Steam Deck yet. The Linux install does not use the
setup exe or the `.bin`.

**One click (Desktop Mode):**
1. Download `AfroSamuraiRecomp-Linux-v0.3.10.desktop` from this release.
2. Double-click it in the file manager. If it asks what to do with the file,
   choose to run (execute) it. If it opens as text instead, right-click it,
   *Properties → Permissions → Is executable*, then double-click it again.
3. A terminal opens. It downloads `afrorecomp-linux-v0.3.10.tar.gz` from this
   release, checks its SHA-256 and runs its `install.sh`. Answer the questions:
   install folder (default `~/Games/AfroSamuraiRecomp`), the path to your ISO
   (you can drag the ISO file onto the terminal window), the Microsoft license
   and the game language.
4. At the end it prints the Steam steps and writes them to
   `<install folder>/STEAM_SETUP.txt`: add `game/afrorecomp.exe` as a non-Steam
   game, set **Start in** and the launch options, and force Proton.

To update an install in `~/Games/AfroSamuraiRecomp` later, download the newer
version's `.desktop` file and double-click it: it updates in place and keeps
the game files, saves and settings.

**From a terminal**, or for an install in another folder:

```bash
tar -xzf afrorecomp-linux-v0.3.10.tar.gz
cd afrorecomp-v0.3.10
./install.sh --iso "/path/to/Afro Samurai (Europe).iso"
# update an existing install:
./install.sh --update --dir "/path/to/AfroSamuraiRecomp"
```

`git clone` of the tag `v0.3.10` gives the same files. Options and details:
README section
[Linux and SteamOS (Proton)](https://github.com/LomoXSoft/AfroSamuraiRecomp#linux-and-steamos-proton).
