# Setup artwork

Lomorsin pixel art, used by the setup program only. It is not game art.

`lomorsin-afro/`:

- `lomorsin_afro.ico`: 16, 24, 32, 48, 64, 128 and 256. Each image is the
  matching `icono_N.png` with no resampling. `lomorsin_afro_512.png` is the
  master and is not inside the icon.
- `cabecera_lomorsin_afro_1200x240.png`: the setup window's header. The left side
  is empty so the localized page title is drawn there.
- `previa_lomorsin_afro.png`: the artist's layout sheet, not a screenshot.

`*.ico` is ignored for game icons, but `tools/artwork/**/*.ico` is not, so the
supplied icon is part of the source. `tools/make_setup_art.py` copies it to
`tools/afrosetup.ico` and encodes the header. The header is scaled with nearest
neighbour (GDI `COLORONCOLOR`) to an integer divisor of 1200x240 that fits the
window.

`glyphs/`: the button icons for each device; see `glyphs/README.md`.
