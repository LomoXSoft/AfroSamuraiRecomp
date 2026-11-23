# AfroSamuraiRecomp v0.3.9 – Prologue soft-lock fix

Source-only release. The downloadable program is the setup installer. It builds
the game on your PC from your own Europe or USA disc. No game executable and no
game files are included.

HDD installation can take a long time; SSD/NVMe recommended.

## Download

Windows: `afrosetup-v0.3.9.exe`

SHA-256 `9f81e783d0f965bfbf2345ced7915bee2a139cb42b6b30e945afb5fc0cd04648`

2110976 bytes.

Already on 0.1.x, 0.2.x or 0.3.x: run it, choose **Update** and pick your install
folder. Your extracted game, saves and launcher settings stay. **Update** rebuilds
only what changed (about 3 minutes from 0.3.8 on the test PC).

Linux and SteamOS (Steam Deck, Steam Machine): `AfroSamuraiRecomp-Linux-v0.3.9.desktop`,
see [Linux and SteamOS](#linux-and-steamos-proton) at the bottom.

## New

### Prologue: the courtyard fight no longer soft-locks
- The original game can soft-lock in the prologue's last fight, the courtyard
  before the boss: an enemy stays on the edge of the field, never comes down and
  never attacks, so the fight never ends.
- Cause: that enemy should walk along the ledge to a waypoint and jump down from
  there. If it ends up below the ledge instead (for example when Afro stands
  where it lands), it keeps walking towards a waypoint 5.5 units above it. The
  game only counts a waypoint as reached within 1.5 units of its height, so the
  enemy waits forever.
- Now, in the prologue only, an enemy that follows a waypoint it can no longer
  reach (more than 1.5 units above or below it) and has not moved for 10
  seconds stops following its waypoints and joins the fight. Enemies on their
  normal path are not touched.
- Settings: `afro_spawn_guard` (on by default), `afro_spawn_guard_s` (10).
  `afro_spawn_diag` logs the spawn managers and their enemies once a second.

### Setup
- The setup no longer lists or opens other processes. Cancel stops only the
  build it started (its job object), and "the game is running" now means this
  install's `game\afrorecomp.exe` is in use, not any `afrorecomp.exe` anywhere.
- `install.ps1` runs with `-ExecutionPolicy RemoteSigned` instead of `Bypass`.
  The scripts the setup writes are local files, so RemoteSigned runs them.
- VirusTotal: this release's setup was flagged by 3 of 70 engines before
  release: Huorong (`Trojan/ShellLoader.gx`), Microsoft
  (`Program:Win32/Wacapew.C!ml`) and SecureAge (`Malicious`). These are
  false positives. The v0.3.8 setup was flagged by 2 (Huorong, AliCloud).
  Machine-learning engines change their guess with each build; a test build
  that stored the files differently scored 5 and was not used. False-positive
  reports go to the engines that flag it.
- If an antivirus warns: the setup is not signed and starts PowerShell to
  download the build tools. Its source is `tools/afrosetup.cpp`, and the SHA-256
  above lets you check your download.

## Tested

- Prologue fix, Europe disc, played: in the courtyard fight an enemy ended up
  below the ledge 5.5 units under its waypoint and stood still. Before the fix
  the fight could not end; with it, the log showed `afro spawn guard: enemy …
  joins the fight` after 10 seconds, the enemy came down and fought, and the
  prologue continued to the boss and the next level.
- Europe, fresh install of this release through the setup's own window, from
  the ISO: 18 minutes. The window showed `afrorecomp_v0.3.9` and the intro movie
  played.
- USA, fresh install of this release through the setup's own window, from an
  extracted USA game folder: region set to USA, 12 minutes. The game showed
  `afrorecomp_v0.3.9` and the intro movie played.
- Update of a 0.3.8 install (made with the released 0.3.8 setup) to 0.3.9
  through the setup's window: 2.8 minutes (the game code is generated again
  because the game hooks changed), launcher settings kept. The game showed
  `afrorecomp_v0.3.9` and the intro movie played.
- `install.ps1` under `-ExecutionPolicy RemoteSigned`: the installs above ran
  with it, and the Process scope it sets overrides a stricter user setting.
- The checks in `checks/` pass, including the setup command test and the
  packaging check (no process-listing imports, RemoteSigned in the command
  line) on the built setup. Saves were unchanged by the tests.

## Still not claimed

- Antivirus results can change after release, and some engines may flag the
  setup.
- The prologue fix was played on the Europe disc. On the USA disc the code it
  relies on was compared with the Europe code (identical apart from moved
  addresses), but that fight was not played.
- The Steam Deck was not tested, and `install.sh` is unchanged from v0.3.7.
- RecompLauncher was not run with this release.
- No install time is guaranteed on HDD, SSD, or NVMe.

## Linux and SteamOS (Proton)

The Linux install builds the same **Windows** game from your own disc, and Steam
runs it through Proton. About 20 GB must be free. Tested on a Steam Machine
(v0.3.7); not tested on a Steam Deck yet.

**One click (Desktop Mode):**
1. Download `AfroSamuraiRecomp-Linux-v0.3.9.desktop` from this release.
2. Double-click it in the file manager. If it asks what to do with the file,
   choose to run (execute) it. If it opens as text instead, right-click it,
   *Properties → Permissions → Is executable*, then double-click it again.
3. A terminal opens. It downloads `afrorecomp-linux-v0.3.9.tar.gz` from this
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
tar -xzf afrorecomp-linux-v0.3.9.tar.gz
cd afrorecomp-v0.3.9
./install.sh --iso "/path/to/Afro Samurai (Europe).iso"
# update an existing install:
./install.sh --update --dir "/path/to/AfroSamuraiRecomp"
```

`git clone` of the tag `v0.3.9` gives the same files. Options and details:
README section
[Linux and SteamOS (Proton)](https://github.com/LomoXSoft/AfroSamuraiRecomp#linux-and-steamos-proton).
