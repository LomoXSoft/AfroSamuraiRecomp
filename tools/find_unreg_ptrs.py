import re,struct,sys
reg=set(int(x,16) for x in re.findall(r'0x(82[0-9A-Fa-f]{6})',open(sys.argv[1]).read()))
d=open(sys.argv[2],'rb').read(); B=0x82000000; lo,hi=0x82150000,0x828419F4
out=set()
for o in range(0,len(d)-3,4):
    va=B+o
    if lo<=va<hi: continue
    v=struct.unpack_from('>I',d,o)[0]
    if lo<=v<hi and v%4==0 and v not in reg:
        p=struct.unpack_from('>I',d,v-B-4)[0]
        if p in (0x4e800020,0x4e800420,0) or (p>>26==18 and p&1==0):
            out.add(v)
print(len(reg),'registered;',len(out),'candidates', file=sys.stderr)
for v in sorted(out): print(f'0x{v:08X}')
