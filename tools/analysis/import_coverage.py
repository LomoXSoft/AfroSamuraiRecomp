#!/usr/bin/env python3
"""Classify XEX imports against ReXGlue kernel sources.
usage: import_coverage.py default.xex default.pe rexglue-src out.md"""
import sys,struct,re,pathlib,collections
xex,pe,src,out=sys.argv[1:5]
d=open(xex,'rb').read(); img=open(pe,'rb').read()
o=0x25b0
strsz,nlib=struct.unpack('>II',d[o+4:o+12])
strs=d[o+12:o+12+strsz].split(b'\0'); names=[s.decode() for s in strs if s]
p=o+12+strsz; libs=[]
for _ in range(nlib):
    size=struct.unpack('>I',d[p:p+4])[0]
    ni,cnt=struct.unpack('>HH',d[p+36:p+40])
    recs=struct.unpack('>%dI'%cnt,d[p+40:p+40+4*cnt]); libs.append((names[ni],recs)); p+=size
tab={}
for lib,f in (('xboxkrnl','xboxkrnl'),('xam','xam')):
    for m in re.finditer(r'XE_EXPORT\(\w+,\s*0x([0-9A-Fa-f]+),\s*(\w+),\s*(\w+)',open(f'{src}/src/kernel/{f}/export_table.inc').read()):
        tab[(lib,int(m[1],16))]=(m[2],m[3])
impl=set();stub=set()
for fp in pathlib.Path(src,'src').rglob('*.cpp'):
    t=fp.read_text(errors='ignore')
    for m in re.finditer(r'REX_(EXPORT|EXPORT_STUB|EXPORT_STUB_RETURN|STUB|STUB_RETURN|STUB_LOG|HOOK|HOOK_RAW)\(\s*__imp__(\w+)',t):
        (stub if 'STUB' in m[1] else impl).add(m[2])
    for m in re.finditer(r'\b(\w+)_entry\b.*?\{\s*\n?\s*(?:return\s+[\w:]+;|\}|REXKRNL_WARN)',t): pass
    for m in re.finditer(r'(?:RegisterVariable|kVariable|variable)\w*\W+"?(\w+)',t): impl.add(m[1])
varsrc=set(re.findall(r'\w+',''.join(pathlib.Path(src,'src',x).read_text() for x in ('kernel/xboxkrnl/xboxkrnl_module.cpp','system/kernel_state.cpp','kernel/xboxkrnl/xboxkrnl_video.cpp','kernel/xboxkrnl/xboxkrnl_debug.cpp','kernel/xboxkrnl/xboxkrnl_threading.cpp'))))
rows=[];cnt=collections.Counter()
for lib,recs in libs:
    lib=lib.split('.')[0]
    for r in recs:
        v=struct.unpack('>I',img[r-0x82000000:r-0x82000000+4])[0]
        typ,ordn=v>>24,v&0xffff
        if typ==1 and lib=='xboxkrnl' and any(x[2]==ordn and x[3]==0 for x in rows if x[0]==lib): pass
        nm,kind=tab.get((lib,ordn),('?','?'))
        st='implementado' if nm in impl else 'stub' if nm in stub else 'falta'
        if kind=='kVariable':
            st='variable inicializada' if nm in varsrc else 'variable (sin init)'
        rows.append((lib,nm,ordn,typ,st,kind))
# dedupe: thunk + var records share ordinal
seen={};
for r in rows:
    k=(r[0],r[2])
    if k not in seen or r[3]==1: seen[k]=r
rows=sorted(seen.values(),key=lambda r:(r[0],r[2]))
for r in rows: cnt[(r[0],r[4])]+=1
with open(out,'w') as f:
    f.write('# Cobertura de imports (ReXGlue %s)\n\n'%src.split('/')[-1])
    for lib in ('xboxkrnl','xam'):
        tot=sum(v for (l,_),v in cnt.items() if l==lib)
        f.write(f'- **{lib}**: {tot} — '+', '.join(f'{s}: {v}' for (l,s),v in sorted(cnt.items()) if l==lib)+'\n')
    f.write('\n| lib | ordinal | nombre | tipo | estado |\n|---|---|---|---|---|\n')
    for r in rows: f.write(f'| {r[0]} | 0x{r[2]:03X} | {r[1]} | {r[5]} | {r[4]} |\n')
print(cnt)
