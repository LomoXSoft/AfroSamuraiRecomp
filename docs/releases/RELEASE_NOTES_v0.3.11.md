# AfroSamuraiRecomp v0.3.11 – Achievements page

Source-only release. The downloadable program is the setup installer. It builds
the game on your PC from your own Europe or USA disc. No game executable and no
game files are included.

HDD installation can take a long time; SSD/NVMe recommended.

## Download

Windows: `afrosetup-v0.3.11.exe`

SHA-256 `525673bfffafa531282bf8500a9a83ec9da868f83e54a12b4097abd0b2e79b96`

412672 bytes. VirusTotal at release: 2 of 70 engines, both machine-learning
guesses (Microsoft `Trojan:Win32/Wacatac.B!ml`, Trapmine
`Malicious.moderate.ml.score`), the same two as v0.3.10.

The setup downloads `afrosetup-v0.3.11-files.bin` from this release by itself
when you press **Install** (SHA-256
`005ad4ae98e67d86010ae81166b0d6b7b84fcd18be9ccd6afc1c5ac3110b9711`, 1772231
bytes). You only need to download that file yourself if the PC has no internet
at that moment: put it next to the setup.

Already on 0.1.x, 0.2.x or 0.3.x: run it, choose **Update** and pick your install
folder. Your extracted game, saves, achievements and launcher settings stay.

Linux and SteamOS (Steam Deck, Steam Machine): `AfroSamuraiRecomp-Linux-v0.3.11.desktop`,
see [Linux and SteamOS](#linux-and-steamos-proton) at the bottom.

## New

### Achievements in the in-game options
- The game's 48 achievements already worked: the game unlocks them as on the
  Xbox 360 and they are saved in `Documents\afrorecomp\achievements`, on this PC
  only, not on Xbox Live. Until now the list could only be opened with **F7**,
  on the keyboard.
- The in-game options (**F1**, or hold **L3 + R3** on a controller) now have two
  tabs, **Options** and **Achievements**. Press **LB** or **RB** (L1 / R1 on a
  PlayStation pad; keys **1** and **3** in the default key bindings) to switch.
- The Achievements tab shows the whole list: the game's own icon, name,
  description and gamerscore for each one, unlocked ones in full colour, the
  total and a progress bar at the top. Up and down move one row, left and right
  jump five, and **B** (or Esc) goes back to the options.
- The tab only shows the list. It does not unlock, change or save anything.
- Names and descriptions come from the game in its language (English, Spanish,
  French or Italian).

### Options menu on the keyboard
- **Enter** now selects or changes the highlighted row, like Space. In the
  default key bindings Enter is also the game's Start button, so before it
  closed the menu.
- **Esc** on the Achievements tab goes back to the options instead of closing
  the menu.

The setup is unchanged from v0.3.10 apart from the version.

## Tested

- Europe, fresh install through the setup's own window, from the ISO, with the
  `.bin` next to the setup: 13.2 minutes. The game started as
  `afrorecomp_v0.3.11` and the intro played. That install was then updated
  with the final setup of this release (0.5 minutes, the menu rebuilt).
- Update of a fresh v0.3.10 install through the setup's window: 0.7 minutes.
  The game started as `afrorecomp_v0.3.11` and the launcher settings were kept.
- The tabs with a controller on Windows: LB / RB switch between Options and
  Achievements.
- The achievement list, with keyboard keys standing in for the controller
  buttons: in Spanish and French, scrolled through all 48 achievements, accents
  shown, B / Esc back to the options, "Back to the game" closes the menu. The
  game exited normally with and without opening the list. The F7 list was
  checked too.
- Saves and the achievements file were unchanged after the tests.
- The checks in `checks/` pass, including the packaging check on the built
  files.

## Still not claimed

- Not tried on a Steam Deck yet.
- The Achievements tab was not seen in English or Italian yet (the text is in the code for
  all four languages).
- The unlock pop-up during play was not seen in these tests.
- Not tested with the USA disc in this release (the menu code is the same for
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
1. Download `AfroSamuraiRecomp-Linux-v0.3.11.desktop` from this release.
2. Double-click it in the file manager. If it asks what to do with the file,
   choose to run (execute) it. If it opens as text instead, right-click it,
   *Properties → Permissions → Is executable*, then double-click it again.
3. A terminal opens. It downloads `afrorecomp-linux-v0.3.11.tar.gz` from this
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
tar -xzf afrorecomp-linux-v0.3.11.tar.gz
cd afrorecomp-v0.3.11
./install.sh --iso "/path/to/Afro Samurai (Europe).iso"
# update an existing install:
./install.sh --update --dir "/path/to/AfroSamuraiRecomp"
```

`git clone` of the tag `v0.3.11` gives the same files. Options and details:
README section
[Linux and SteamOS (Proton)](https://github.com/LomoXSoft/AfroSamuraiRecomp#linux-and-steamos-proton).
