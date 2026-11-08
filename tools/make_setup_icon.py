#!/usr/bin/env python3
"""Retired. The katana mark must not replace the supplied Lomorsin icon."""

import pathlib
import runpy
import sys

sys.stderr.write(
    "make_setup_icon.py no longer draws an icon. "
    "make_setup_art.py copies tools/artwork/lomorsin-afro/lomorsin_afro.ico.\n"
)
runpy.run_path(str(pathlib.Path(__file__).with_name("make_setup_art.py")), run_name="__main__")
