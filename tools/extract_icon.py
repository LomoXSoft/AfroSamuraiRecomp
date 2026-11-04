#!/usr/bin/env python3
"""Extract the title image (XDBF image id 0x8000, 64x64 PNG) from the game's
decrypted executable and write a PNG-in-ICO icon. Game-derived output: keep
private (gitignored). usage: extract_icon.py default.pe app/afro_icon.ico"""
import struct, sys
d = open(sys.argv[1], 'rb').read()
i = d.find(b'XDBF')
if i < 0: sys.exit('XDBF not found')
_, _, tlen, tnum, flen, _ = struct.unpack('>6I', d[i:i + 24])
base = i + 24 + tlen * 18 + flen * 8
png = None
for k in range(tnum):
    ns, rid, off, ln = struct.unpack('>HQII', d[i + 24 + k * 18:i + 42 + k * 18])
    if ns == 2 and rid == 0x8000:
        png = d[base + off:base + off + ln]
if not png or png[:4] != b'\x89PNG': sys.exit('title image not found')
w, h = struct.unpack('>II', png[16:24])
ico = struct.pack('<HHH', 0, 1, 1) + struct.pack('<BBBBHHII', w % 256, h % 256, 0, 0, 1, 32, len(png), 22) + png
open(sys.argv[2], 'wb').write(ico)
print('icon %dx%d, %d bytes -> %s' % (w, h, len(png), sys.argv[2]))
