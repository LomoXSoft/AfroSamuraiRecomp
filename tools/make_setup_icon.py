#!/usr/bin/env python3
"""Original charcoal/red mark for the setup program. Not game art."""

from __future__ import annotations

import pathlib
import struct


def bitmap(size: int) -> bytes:
    pixels = []
    for y in range(size - 1, -1, -1):
        for x in range(size):
            # Charcoal field, a red katana diagonal, a short white stroke.
            r, g, b = 28, 28, 30
            if abs((x + y) - (size - 4)) <= max(1, size // 16) and x > size // 5:
                r, g, b = 196, 40, 42
            if y == size // 3 and size // 6 < x < size // 2:
                r, g, b = 236, 236, 236
            pixels.append(struct.pack("<BBBB", b, g, r, 255))
    xor_bytes = b"".join(pixels)
    header = struct.pack("<IiiHHIIiiII", 40, size, size * 2, 1, 32, 0, len(xor_bytes), 0, 0, 0, 0)
    and_row = ((size + 31) // 32) * 4
    return header + xor_bytes + (b"\x00" * (and_row * size))


def main() -> None:
    sizes = (16, 32, 48)
    images = [bitmap(s) for s in sizes]
    out = pathlib.Path(__file__).with_name("afrosetup.ico")
    parts = [struct.pack("<HHH", 0, 1, len(sizes))]
    offset = 6 + 16 * len(sizes)
    entries = []
    blobs = []
    for size, image in zip(sizes, images):
        entries.append(struct.pack("<BBBBHHII", size if size < 256 else 0, size if size < 256 else 0, 0, 0, 1, 32, len(image), offset))
        offset += len(image)
        blobs.append(image)
    out.write_bytes(b"".join(parts + entries + blobs))
    print(out, out.stat().st_size)


if __name__ == "__main__":
    main()
