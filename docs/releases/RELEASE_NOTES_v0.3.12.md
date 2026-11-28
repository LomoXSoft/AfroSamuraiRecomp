# AfroSamuraiRecomp v0.3.12 – Installs work again, prologue fight fix

Source-only release. The downloadable program is the setup installer. It builds
the game on your PC from your own Europe or USA disc. No game executable and no
game files are included.

HDD installation can take a long time; SSD/NVMe recommended.

**New installs of v0.3.11 and older fail** at the ReXGlue SDK step with a
404 error, on Windows and Linux. Use this version. Updating an existing install
was not affected.

## Download

Windows: `afrosetup-v0.3.12.exe`

SHA-256 `290690a45144af387bd3f096026090d9f23a549cc903f0b7fd901ba9ed11e064`

412672 bytes. Not checked on VirusTotal for this release.

The setup downloads `afrosetup-v0.3.12-files.bin` from this release by itself
when you press **Install** (SHA-256
`3224c864ad8eee706f6bff8f187de7932f7d2c6bf0270d7211d59d0938f9f17e`, 1783557
bytes). You only need to download that file yourself if the PC has no internet
at that moment: put it next to the setup.

Already on 0.1.x, 0.2.x or 0.3.x: run it, choose **Update** and pick your install
folder. Your extracted game, saves, achievements and launcher settings stay.

Linux and SteamOS (Steam Deck, Steam Machine): `AfroSamuraiRecomp-Linux-v0.3.12.desktop`,
see [Linux and SteamOS](#linux-and-steamos-proton) at the bottom.

## Fixed

### New installs failed with a 404 at the ReXGlue SDK step
- The installers download a prebuilt ReXGlue SDK nightly build from
  2 October 2026. ReXGlue keeps only its last ten nightly builds and has deleted
  that one, so every new install stopped with "(404) Not Found".
- The same two SDK zips (same SHA-256, BSD-3-Clause license) are now kept on
  this repository's release
  [rexglue-sdk-bd833a2](https://github.com/LomoXSoft/AfroSamuraiRecomp/releases/tag/rexglue-sdk-bd833a2).
  The Windows setup and the Linux installer download them from there first and
  from ReXGlue second. The game is built with exactly the same SDK as before.

### Prologue: the courtyard fight could still get stuck (issue #7)
- v0.3.9 added a fix for an original game bug in the last fight before the
  boss: an enemy that ends up below the ledge it should walk along waits there
  forever. After 10 seconds the fix makes it join the fight.
- The same fix also caught the enemy that waits on the roof and then jumps down.
  If it stood at the roof edge for 10 seconds, waiting for Afro to come closer,
  it was made to fight up there. It could not reach Afro and never came down,
  so the fight never ended.
- The fix now only acts on enemies below their target, as in the original bug.
  An enemy above its target is left alone, as in the original game.

## Tested

- The prologue courtyard fight on Windows, Europe disc, 60 FPS, with the
  enemy log on (`--afro_spawn_diag=true`), after updating to this version:
  - all three enemies that got stuck below the ledge were released ("below
    its waypoint", 5.5 to 5.7 units) and the fight ended.
  - The enemy on the roof jumped down by itself and fought.
- The same fight with the fix off (`--afro_spawn_guard=false`): those three
  enemies stayed stuck, and the enemy on the roof jumped down by itself.
- The roof-edge wait from issue #7 (the roof enemy standing at the edge for 10
  seconds) did not happen in these runs, so this release does not prove that
  case on its own; the reporter's log shows the old fix caused that lock.
- New install on Windows with `afrosetup-v0.3.12.exe`, Europe disc, into an
  empty folder: the setup downloaded the ReXGlue SDK from the new release,
  checked its SHA-256 and finished ("INSTALACION OK", 21 min).
- New install on Linux (WSL, Ubuntu) with `afrorecomp-linux-v0.3.12.tar.gz`,
  Europe disc, into an empty folder: `install.sh` downloaded both SDK zips from
  the new release and built `game/afrorecomp.exe` (12 min). Not run under
  Proton in this release.
- The new installs did not touch the saves or achievements.
- The repository checks (`checks/`) pass.

## Still not claimed

- Not tried on a Steam Deck yet.
- Not tested with the USA disc in this release (the changed code is the same for
  both discs).
- Antivirus results can change after release, and some engines may flag the
  setup.
- The setup is not signed.
- RecompLauncher was not run with this release.
- No install time is guaranteed on HDD, SSD, or NVMe.

## Linux and SteamOS (Proton)

The Linux install builds the same **Windows** game from your own disc, and Steam
runs it through Proton. About 20 GB must be free. Tested on a Steam Machine
(v0.3.7); not tested on a Steam Deck yet. The Linux install does not use the
setup exe or the `.bin`.

**One click (Desktop Mode):**
1. Download `AfroSamuraiRecomp-Linux-v0.3.12.desktop` from this release.
2. Double-click it in the file manager. If it asks what to do with the file,
   choose to run (execute) it. If it opens as text instead, right-click it,
   *Properties → Permissions → Is executable*, then double-click it again.
3. A terminal opens. It downloads `afrorecomp-linux-v0.3.12.tar.gz` from this
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
tar -xzf afrorecomp-linux-v0.3.12.tar.gz
cd afrorecomp-v0.3.12
./install.sh --iso "/path/to/Afro Samurai (Europe).iso"
# update an existing install:
./install.sh --update --dir "/path/to/AfroSamuraiRecomp"
```

`git clone` of the tag `v0.3.12` gives the same files. Options and details:
README section
[Linux and SteamOS (Proton)](https://github.com/LomoXSoft/AfroSamuraiRecomp#linux-and-steamos-proton).
