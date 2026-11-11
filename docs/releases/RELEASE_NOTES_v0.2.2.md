# AfroSamuraiRecomp v0.2.2 – Options while playing

Source-only release. The downloadable program is the setup installer. It builds
the game on your PC from your own Europe disc. No game executable and no game
files are included.

HDD installation can take a long time; SSD/NVMe recommended.

## Download

`afrosetup-v0.2.2.exe`

SHA-256 `252ab7c900daa8999c5dcc25af9e0496db4c83c41052d0933b9167ca7831ae46`

1889190 bytes.

Already on 0.1.x or 0.2.x: run it, choose **Update** and pick your install folder.
Your extracted game, saves and launcher settings stay. **Update** rebuilds only
what changed.

## New

### Options while playing
- Press **F1**, or hold both stick clicks (**L3 + R3**) for half a second, to open
  the options over the paused game. It is drawn in the game's own style: red ink
  strokes, the selected row in a red stroke with arrows, and the same button icons
  as the game's prompts for the device you are using (Xbox, PlayStation, Switch,
  Steam, or your bound keys on the keyboard).
- It has the launcher's main settings: display, frame rate, VSync, internal
  resolution, difficulty, loading screens, language, control mode, the three mouse
  sliders and the Experimental effects. Each change is saved at once; most also
  apply at once, and internal resolution, language and control mode apply the
  next time the game starts.
- The game's own progress stats (L3 + R3 while playing) stay readable on the left
  while the menu is open.
- Move with the D-pad or the arrow keys, change with left and right, and close with
  B, Start, Back, Esc or F1 again. Key bindings stay in the launcher.

## Tested

- Fresh install into an empty folder with this setup (13 min): the game built
  with the menu's runtime functions and started from a RecompLauncher-style start.
- Played on the author's PC: the menu with a controller and the keyboard, display
  and frame rate changed from it, the game's stats readable beside it, and the
  button icons following the device in use.
- The whole patch stack applies to a clean ReXGlue `bd833a2`.

## Still not claimed

- At 60, scenes that show two camera views at once can still drop below 60.
- Only the Justice and Okiku loading stories were checked one by one; others follow
  the same two rules.
- No install time is guaranteed on HDD, SSD, or NVMe.
