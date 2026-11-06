# AfroSamuraiRecomp v0.1.0 – Fully playable

Source-only release. The downloadable program is the setup installer. It builds
the game on your PC from your own Europe disc. No game executable and no game
files are included.

## Highlights

- Original (30) is the default. On the machines exercised by the author, the game starts from the installer and is playable.
- A graphical setup program, `afrosetup.exe`, in English, Español, Français, and Italiano. It can install, or update an existing 0.1.0 folder, without administrator rights and without changing PATH or the registry.
- `install.bat` remains the console installer.
- Launcher choices use the labels `30 (original)`, `60 (experimental)`, `RTV (original)`, and `ROV (experimental)`.
- Cheats are a separate dialog. Invulnerability is off unless you turn it on for that session, and it is not saved.

## Changes

- Setup asks for language, then Install or Update, then the folder, your Europe ISO or an already extracted game, and the Microsoft Build Tools license (the box starts unchecked).
- Update keeps the extracted game, saves, and launcher settings. It rebuilds even when the version is still 0.1.0, reuses build tools that are still on disk, and downloads again only the tools that cleanup removed.
- The installed `game\Play.bat` is what Jugar starts, and only after you click it.
- Experimental (60) remains opt-in.
- Invulnerability (experimental) is still in the tree as a session cheat. It is not a fix.

## Known issues

- Frame rate dips, especially above native internal resolution.
- Black textures, including the headband.
- Occasional odd audio.
- Experimental (60) can accelerate cutscenes and still drop frames. It is not a finished 60 FPS mode.
- Invulnerability (experimental) does not currently prevent death. Leave it off unless you are testing that cheat.

## Building

Run `afrosetup.exe`, or `install.bat` from a source checkout. Both compile from your own Afro Samurai (Europe) ISO. The setup program already contains this 0.1.0 source. It does not download a prebuilt game.

## Tested on

- One Windows PC: a clean install with the first 0.1.0 setup program. The author reports that the game works.
- The same PC: a same-version update with the later setup program. Jugar starts the game, and the result seemed fine.
- This is the author's use, not an independent check of the whole game.
- The Cheats warning text and the shorter installer progress line in this package were not run again on Windows after they were added.

## Legal

Fan project, not affiliated with Namco Bandai, GONZO, Microsoft, or other rights holders. No game code or assets are distributed. GPL-3.0 for this repository except `patches/rexglue/` (BSD-3). You need your own original game.
