# Setup artwork for private 0.1.1

Lomorsin pixel art for the setup program only. It is not game art and it does
not change gameplay. The published v0.1.0 release is unchanged.

Original files live in `tools/artwork/lomorsin-afro/`:

- `lomorsin_afro.ico` — 16, 24, 32, 48, 64, 128, and 256. Each image is the
  matching `icono_N.png` with no resampling. `lomorsin_afro_512.png` is the
  master and is not inside the icon.
- `cabecera_lomorsin_afro_1200x240.png` — wizard header. The left side is empty
  so the localized page title is drawn there. The character stays on the right.
- `previa_lomorsin_afro.png` — the artist's layout sheet, not a screenshot of
  this program.

`*.ico` stays ignored for game icons. `tools/artwork/**/*.ico` is not ignored,
so the supplied icon is part of the source. `tools/build_setup.sh` copies that
icon to `tools/afrosetup.ico` and does not draw a new one. `make_setup_icon.py`
no longer paints the old katana mark.

The header is scaled with nearest neighbor (GDI `COLORONCOLOR`) to an integer
divisor of 1200×240 that fits the top of the window. It is not smoothed.
