# AfroSamuraiRecomp v0.3.7 – Linux installer (Proton)

Source-only release. The downloadable program is the setup installer. It builds
the game on your PC from your own Europe or USA disc. No game executable and no
game files are included.

HDD installation can take a long time; SSD/NVMe recommended.

## Download

Windows: `afrosetup-v0.3.7.exe`

SHA-256 `e0408841e32272c94e08ea90fef4b6bf90e66b1f7d53e84228dc117e30f5c02a`

2060156 bytes.

Already on 0.1.x, 0.2.x or 0.3.x: run it, choose **Update** and pick your install
folder. Your extracted game, saves and launcher settings stay. **Update** rebuilds
only what changed (under a minute from 0.3.6 on the test PC).

Linux, SteamOS (Steam Deck, Steam Machine): there is no file to download. Get
the source with git and run `install.sh`, as in the README section
[Linux and SteamOS (Proton)](https://github.com/LomoXSoft/AfroSamuraiRecomp#linux-and-steamos-proton).

## New

### Linux and SteamOS through Proton
- `install.sh` builds the same **Windows** game on Linux from your own disc
  (Europe or USA), and Steam runs it through Proton. It is not a native Linux
  port.
- It needs no root and installs no packages: bash, python3, git, curl, tar and
  xz are enough, which SteamOS has. Every tool is downloaded pinned and SHA-256
  checked into the install folder. About 20 GB must be free.
- In Desktop Mode, in a terminal:

  ```bash
  git clone https://github.com/LomoXSoft/AfroSamuraiRecomp.git
  cd AfroSamuraiRecomp
  ./install.sh --iso "/path/to/Afro Samurai (Europe).iso"
  ```

  At the end it prints the Steam steps (add `game/afrorecomp.exe` as a
  non-Steam game, set its start folder and launch options, force Proton) and
  writes them to `STEAM_SETUP.txt` in the install folder.
- The launcher window has no controller support, so a Linux install skips it.
  The options are in the game: **F1**, or hold both stick clicks.
- `./install.sh --update --dir <install folder>` after a `git pull` keeps the
  game files, saves and settings and compiles only what changed.

The Windows setup and the game are the same as in v0.3.6.

## Tested

- Steam Machine, under Proton, from a test archive with the same `install.sh`
  and Linux tools: the install and the Steam steps worked, and the game runs in
  60 FPS mode at 1440p, with some drops to 40–50 FPS in places.
- Linux install in WSL (Ubuntu) from the Europe ISO, before that: about 9.5
  minutes with downloads, nothing written outside the install folder. The files
  it built also ran on Windows.
- Windows, Europe, fresh install of this release through the setup's own window,
  from the ISO: the ISO matched Redump 17884, 16 minutes. Started past the
  launcher: the window showed `afrorecomp_v0.3.7` and the intro movie played.
- Windows, update of a 0.3.6 Europe install to 0.3.7: about half a minute, code
  generation skipped, launcher settings kept. The game showed
  `afrorecomp_v0.3.7` and the intro movie played.
- The checks in `checks/` pass, including the Linux installer check and the
  setup command test.

## Still not claimed

- The Steam Deck was not tested. The build time there is an estimate.
- On Linux, the USA disc was not tested, and `--update` was tested only in
  WSL, not on the Steam Machine.
- On Windows, the USA disc was not installed with this release (no USA files
  on the test PC). The setup passes the same options for both discs, and the
  game is unchanged from v0.3.6.
- Antivirus results can change after release, and some engines may flag the
  setup.
- RecompLauncher was not run with this release.
- No install time is guaranteed on HDD, SSD, or NVMe.
