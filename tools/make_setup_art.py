#!/usr/bin/env python3
"""Copy the supplied Lomorsin icon and encode the header for the setup window.

This does not draw a replacement icon. tools/afrosetup.ico is a byte copy of
tools/artwork/lomorsin-afro/lomorsin_afro.ico. The header pixels are the
1200x240 PNG with no resampling; the window scales them with nearest neighbor.
"""

from __future__ import annotations

import pathlib
import struct
import zlib

ROOT = pathlib.Path(__file__).resolve().parent
ART = ROOT / "artwork" / "lomorsin-afro"
ICON_SRC = ART / "lomorsin_afro.ico"
HEADER_SRC = ART / "cabecera_lomorsin_afro_1200x240.png"
ICON_OUT = ROOT / "afrosetup.ico"
HEADER_OUT = ROOT / "afrosetup_header.inc"

ICON_SIZES = (16, 24, 32, 48, 64, 128, 256)


def fail(msg: str) -> None:
    raise SystemExit(f"setup art: {msg}")


def png_rgba(path: pathlib.Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        fail(f"{path.name} is not a PNG")
    off = 8
    width = height = None
    depth = color = None
    idat = b""
    while off + 8 <= len(data):
        size = struct.unpack(">I", data[off : off + 4])[0]
        kind = data[off + 4 : off + 8]
        chunk = data[off + 8 : off + 8 + size]
        if kind == b"IHDR":
            width, height, depth, color = struct.unpack(">IIBB", chunk[:10])
        elif kind == b"IDAT":
            idat += chunk
        elif kind == b"IEND":
            break
        off += 12 + size
    if width is None or depth != 8 or color != 6:
        fail(f"{path.name} must be 8-bit RGBA, got {width}x{height} depth {depth} color {color}")
    raw = zlib.decompress(idat)
    stride = width * 4
    rows = []
    prev = bytearray(stride)
    pos = 0
    for _ in range(height):
        filt = raw[pos]
        pos += 1
        row = bytearray(raw[pos : pos + stride])
        pos += stride
        if filt == 1:
            for i in range(stride):
                left = row[i - 4] if i >= 4 else 0
                row[i] = (row[i] + left) & 255
        elif filt == 2:
            for i in range(stride):
                row[i] = (row[i] + prev[i]) & 255
        elif filt == 3:
            for i in range(stride):
                left = row[i - 4] if i >= 4 else 0
                row[i] = (row[i] + ((left + prev[i]) // 2)) & 255
        elif filt == 4:
            for i in range(stride):
                a = row[i - 4] if i >= 4 else 0
                b = prev[i]
                c = prev[i - 4] if i >= 4 else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if pa <= pb and pa <= pc else b if pb <= pc else c
                row[i] = (row[i] + pr) & 255
        elif filt != 0:
            fail(f"unsupported PNG filter {filt} in {path.name}")
        rows.append(bytes(row))
        prev = row
    return width, height, b"".join(rows)


def check_icon() -> None:
    blob = ICON_SRC.read_bytes()
    reserved, kind, count = struct.unpack_from("<HHH", blob, 0)
    if (reserved, kind, count) != (0, 1, len(ICON_SIZES)):
        fail(f"icon directory is {count} images, expected {len(ICON_SIZES)}")
    for index, size in enumerate(ICON_SIZES):
        entry = 6 + 16 * index
        w, h, _colors, _res, _planes, bits, nbytes, offset = struct.unpack_from("<BBBBHHII", blob, entry)
        shown_w = w or 256
        shown_h = h or 256
        if shown_w != size or shown_h != size or bits != 32:
            fail(f"icon entry {index} is {shown_w}x{shown_h} {bits}bpp, expected {size}x{size} 32bpp")
        png = (ART / f"icono_{size}.png").read_bytes()
        got = blob[offset : offset + nbytes]
        if got != png:
            fail(f"icon entry {size} is not a byte copy of icono_{size}.png")
        width, height, _pixels = png_rgba(ART / f"icono_{size}.png")
        if (width, height) != (size, size):
            fail(f"icono_{size}.png is {width}x{height}")
    master = png_rgba(ART / "lomorsin_afro_512.png")
    if master[0] != 512 or master[1] != 512:
        fail("512 master is not 512x512")


def rle(pixels: bytes) -> list[tuple[int, int]]:
    runs = []
    count = 0
    current = None
    for i in range(0, len(pixels), 4):
        r, g, b, a = pixels[i : i + 4]
        bgra = b | (g << 8) | (r << 16) | (a << 24)
        if bgra == current and count < 65535:
            count += 1
            continue
        if current is not None:
            runs.append((count, current))
        current = bgra
        count = 1
    if current is not None:
        runs.append((count, current))
    return runs


def main() -> None:
    if not ICON_SRC.is_file() or not HEADER_SRC.is_file():
        fail("artwork files are missing; refusing to draw a replacement icon")
    check_icon()
    width, height, pixels = png_rgba(HEADER_SRC)
    if (width, height) != (1200, 240):
        fail(f"header is {width}x{height}, expected 1200x240")
    runs = rle(pixels)
    expanded = 0
    for count, _color in runs:
        expanded += count
    if expanded != width * height:
        fail("header run-length does not cover every pixel")
    lines = [
        "// Generated by make_setup_art.py from cabecera_lomorsin_afro_1200x240.png.",
        "// Pixels are the source PNG. Do not resample this table.",
        "static constexpr int kHeaderW = 1200;",
        "static constexpr int kHeaderH = 240;",
        "struct HeaderRun { unsigned short n; unsigned int bgra; };",
        "static const HeaderRun kHeaderRuns[] = {",
    ]
    for count, color in runs:
        lines.append(f"  {{{count}, 0x{color:08x}u}},")
    lines.append("};")
    lines.append(f"static constexpr int kHeaderRunCount = {len(runs)};")
    HEADER_OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    ICON_OUT.write_bytes(ICON_SRC.read_bytes())
    if ICON_OUT.read_bytes() != ICON_SRC.read_bytes():
        fail("icon copy does not match the supplied file")
    print(f"icon {ICON_OUT} {ICON_OUT.stat().st_size} bytes (copy)")
    print(f"header {width}x{height} {len(runs)} runs")


if __name__ == "__main__":
    main()
