#!/usr/bin/env python3
"""Decrypt/decompress a retail XEX2 (encryption: normal, compression: none or
basic) into its PE basefile, needed by gen_switch_tables.py and extract_icon.py.
The retail XEX key is not stored here: it is read from the ReXGlue SDK source
(src/system/xex_module.cpp, xe_xex2_retail_key). Output is game-derived: keep
it private. Requires: pip install pycryptodome
usage: xex_to_pe.py default.xex default.pe path/to/rexglue-sdk"""
import re, struct, sys
from pathlib import Path
try:
    from Crypto.Cipher import AES
except ImportError:
    sys.exit('xex_to_pe.py needs pycryptodome: run  python -m pip install pycryptodome')

xex_path, out_path, sdk = sys.argv[1], sys.argv[2], Path(sys.argv[3])
src = (sdk / 'src/system/xex_module.cpp').read_text(errors='ignore')
m = re.search(r'xe_xex2_retail_key\[16\]\s*=\s*\{([^}]*)\}', src)
if not m: sys.exit('retail key not found in ' + str(sdk))
key = bytes(int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', m.group(1)))
assert len(key) == 16

d = open(xex_path, 'rb').read()
magic, _, pe_off, _, sec_off, nopt = struct.unpack('>4sIIIII', d[:24])
if magic != b'XEX2': sys.exit('not an XEX2 file')
hdr = {}
for i in range(nopt):
    k, v = struct.unpack('>II', d[24 + i * 8:32 + i * 8]); hdr[k] = v
ff = hdr[0x3FF]
ff_size, enc, comp = struct.unpack('>IHH', d[ff:ff + 8])
# security info: header size, image size, rsa sig (256), unk, flags, load addr,
# sha1 (20), import table count, sha1 (20), media id (16), file key (16)
image_size = struct.unpack('>I', d[sec_off + 4:sec_off + 8])[0]
o = sec_off + 8 + 256 + 12 + 20 + 4 + 20 + 16
file_key = d[o:o + 16]
data = d[pe_off:]
if enc == 1:
    session = AES.new(key, AES.MODE_ECB).decrypt(file_key)
    data = AES.new(session, AES.MODE_CBC, iv=bytes(16)).decrypt(data[:len(data) // 16 * 16])
elif enc != 0:
    sys.exit('unsupported encryption %d' % enc)
if comp == 1:  # basic: (data_size, zero_size) blocks
    out = bytearray(); p = 0
    for i in range((ff_size - 8) // 8):
        ds, zs = struct.unpack('>II', d[ff + 8 + i * 8:ff + 16 + i * 8])
        out += data[p:p + ds] + bytes(zs); p += ds
    data = bytes(out)
elif comp != 0:
    sys.exit('unsupported compression %d (LZX not supported)' % comp)
data = data[:image_size]
if data[:2] != b'MZ': sys.exit('decryption failed (no MZ header)')
open(out_path, 'wb').write(data)
print('wrote', out_path, len(data), 'bytes')
