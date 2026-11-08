# Button prompt icons

Replacement icons for the game's Xbox button prompts, used by
`patches/rexglue/zzzz-afro-button-glyphs.patch` when the controller is not an
Xbox pad. They are original art drawn by `tools/make_glyphs.ps1` (same license
as the repository). Xbox pads keep the game's own icons, so there is no `xbox`
folder.

The installer copies `tools/artwork/glyphs` next to `afrorecomp.exe` as `glyphs\`.

- `playstation\`, `nintendo\`, `steam\` (Steam Deck and Steam Controller),
  `keyboard\` (key caps and mouse buttons for the default
  keyboard-as-controller bindings: A Space, B E, X left click, Y right click,
  LB 1, RB 3, LT R, RT Q, Back Esc, Start Enter, LS F, RS K).
- Files are named by the Xbox button POSITION the game asks for: `A`, `B`, `X`,
  `Y`, `LB`, `RB`, `LT`, `RT`, `BACK`, `START`, `LS`, `RS`.
- On Nintendo the label is the one at that position: `A.png` shows **B**,
  `B.png` shows **A**, `X.png` shows **Y**, `Y.png` shows **X**.
- Triggers are 128x256 because the game's trigger prompt is 64x128. The rest
  are 128x128. Any size works: the image is fitted and centred.

Identified in game so far: A, B, X, Y and RT. The others are ready for when
their textures are found (`--afro_glyph_log=true`).
