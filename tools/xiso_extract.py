# usage: python xiso_extract.py <iso> <out.zip|outdir> <prefix> [prefix...]   (prefix '/' = all)
# Extracts files whose path starts with any prefix (case-insensitive) into a stored zip.
import struct,sys,zipfile
iso,outz=sys.argv[1],sys.argv[2]; pref=[p.lower() for p in sys.argv[3:]]
f=open(iso,'rb'); base=None
for off in (0,0xFD90000,0x89D80000,0x2080000):
    f.seek(off+0x10000)
    if f.read(20)==b'MICROSOFT*XBOX*MEDIA': base=off;break
f.seek(base+0x10000+20); rs,rz=struct.unpack('<II',f.read(8))
files=[]
def walk(sec,size,path):
    f.seek(base+sec*2048); data=f.read(size); seen=set()
    def node(o):
        if o in seen or o+14>len(data): return
        seen.add(o)
        l,r,s,z,a,n=struct.unpack('<HHIIBB',data[o:o+14])
        if l==0xFFFF: return
        p=path+'/'+data[o+14:o+14+n].decode('latin1')
        if l: node(l*4)
        if a&0x10:
            if z: walk(s,z,p)
        else: files.append((p,s,z))
        if r: node(r*4)
    node(0)
walk(rs,rz,'')
tot=0
import os
if not outz.lower().endswith('.zip'):
    for p,s,z in files:
        if p.startswith('/$SystemUpdate') or not any(p.lower().startswith(x) for x in pref): continue
        dst=os.path.join(outz,*p.strip('/').split('/')); os.makedirs(os.path.dirname(dst),exist_ok=True)
        f.seek(base+s*2048); rem=z
        with open(dst,'wb') as w:
            while rem: c=f.read(min(rem,1<<24)); w.write(c); rem-=len(c)
        tot+=z
    print('extracted',tot,'bytes'); sys.exit(0)
with zipfile.ZipFile(outz,'w',zipfile.ZIP_DEFLATED) as zf:
    for p,s,z in files:
        if any(p.lower().startswith(x) for x in pref):
            f.seek(base+s*2048); zf.writestr(p.strip('/'),f.read(z)); tot+=z
print('extracted',tot,'bytes')
