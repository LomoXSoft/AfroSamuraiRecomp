# AfroSamuraiRecomp v0.3.8 – Setup payload as a resource

Source-only release. The downloadable program is the setup installer. It builds
the game on your PC from your own Europe or USA disc. No game executable and no
game files are included.

HDD installation can take a long time; SSD/NVMe recommended.

## Download

Windows: `afrosetup-v0.3.8.exe`

SHA-256 `8a22776d4dac63699feed2e148a00b1c4176ecd2fe70a0a5a0d34e9ed3967d07`

2087424 bytes.

Already on 0.1.x, 0.2.x or 0.3.x: run it, choose **Update** and pick your install
folder. Your extracted game, saves and launcher settings stay. **Update** rebuilds
only what changed (under a minute from 0.3.7 on the test PC).

Linux and SteamOS (Steam Deck, Steam Machine): `AfroSamuraiRecomp-Linux-v0.3.8.desktop`,
see [Linux and SteamOS](#linux-and-steamos-proton) at the bottom.

## New

### Fewer antivirus warnings for the setup
- VirusTotal flagged the v0.3.7 setup on 3 of 71 engines (Bkav, SecureAge/APEX,
  Symantec), so RecompLauncher held it back.
- The setup now carries its files as a normal Windows resource instead of bytes
  appended after the program, and reads them from memory instead of opening
  its own exe file. Both are traits antivirus engines look for in "droppers".
  Nothing changes for you: the setup unpacks the same files and runs the same
  install.
- A test build of v0.3.7 with only this change was flagged by 1 of 71 engines
  (Huorong). This release's setup scanned 1 of 71 before release: Huorong
  (`Trojan/ShellLoader.gx`). Bkav, SecureAge, Symantec, Microsoft and
  CrowdStrike do not flag it.
- If an antivirus still warns: the setup is not signed and starts PowerShell to
  download the build tools. Its source is `tools/afrosetup.cpp`, and the SHA-256
  above lets you check your download.

### Setup build on GitHub Actions
- `tools/build_release_setup.ps1` builds the setup from one commit, the way
  releases are built. `.github/workflows/setup.yml` runs it on GitHub Actions
  when started by hand, keeps the setup as a download with a build provenance
  attestation, and attaches it only to a draft release. This is the first step
  towards a signed setup.

The game and `install.sh` are the same as in v0.3.7.

## Tested

- Europe, fresh install of this release through the setup's own window, from
  the ISO: the ISO matched Redump 17884. 54 minutes, most of it spent on slow
  downloads that night. The setup unpacked the same 311 files as the release
  source, from its resource. Started past the launcher: the window showed
  `afrorecomp_v0.3.8` and the intro movie played.
- USA, fresh install of this release through the setup's own window, from an
  extracted USA game folder: region set to USA, 64 minutes (slow downloads; the
  xz and ReXGlue SDK archives were copied in from an earlier download and passed
  the SHA-256 check). The game showed `afrorecomp_v0.3.8` and the intro movie
  played.
- Update of a 0.3.7 Europe install (made from the v0.3.7 source with its
  console installer) to 0.3.8 through the setup's window: 0.6
  minutes, code generation skipped, launcher settings kept. The game showed
  `afrorecomp_v0.3.8` and the intro movie played.
- The checks in `checks/` pass, including the setup command test, which reads
  the payload from the built setup's resource and checks that nothing is
  appended after the program.

## Still not claimed

- Antivirus results can change after release, and some engines may flag the
  setup.
- The Steam Deck was not tested, and `install.sh` is unchanged from v0.3.7.
- The `.desktop` launcher was tested in WSL (download, SHA-256 check, new
  install and update start), not yet by double-clicking it on SteamOS.
- RecompLauncher was not run with this release.
- No install time is guaranteed on HDD, SSD, or NVMe.

## Linux and SteamOS (Proton)

The Linux install builds the same **Windows** game from your own disc, and Steam
runs it through Proton. About 20 GB must be free. Tested on a Steam Machine
(v0.3.7); not tested on a Steam Deck yet.

**One click (Desktop Mode):**
1. Download `AfroSamuraiRecomp-Linux-v0.3.8.desktop` from this release.
2. Double-click it in the file manager. If it asks what to do with the file,
   choose to run (execute) it. If it opens as text instead, right-click it,
   *Properties → Permissions → Is executable*, then double-click it again.
3. A terminal opens. It downloads `afrorecomp-linux-v0.3.8.tar.gz` from this
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
tar -xzf afrorecomp-linux-v0.3.8.tar.gz
cd afrorecomp-v0.3.8
./install.sh --iso "/path/to/Afro Samurai (Europe).iso"
# update an existing install:
./install.sh --update --dir "/path/to/AfroSamuraiRecomp"
```

`git clone` of the tag `v0.3.8` gives the same files. Options and details:
README section
[Linux and SteamOS (Proton)](https://github.com/LomoXSoft/AfroSamuraiRecomp#linux-and-steamos-proton).
