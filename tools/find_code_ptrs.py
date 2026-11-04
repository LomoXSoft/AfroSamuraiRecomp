# Find code addresses materialized by lis/addi (or lis/ori) that are not registered functions.
import re,struct,sys
reg=set(int(x,16) for x in re.findall(r'0x(82[0-9A-Fa-f]{6})',open(sys.argv[1]).read()))
d=open(sys.argv[2],'rb').read(); B=0x82000000; lo,hi=0x82150000,0x828419F4
W=lambda a: struct.unpack_from('>I',d,a-B)[0]
hits={}
for a in range(lo,hi,4):
    i=W(a)
    if i>>26!=15 or (i>>16)&31: continue  # lis rD,imm (addis rD,0,imm)
    rd=(i>>21)&31; h=(i&0xffff)<<16
    for b in range(a+4,min(a+40,hi),4):
        j=W(b); op=j>>26
        if op in(14,24) and (j>>16)&31==rd:
            lo16=j&0xffff
            v=(h+(lo16-0x10000 if op==14 and lo16&0x8000 else lo16))&0xffffffff
            if lo<=v<hi and v%4==0 and v not in reg: hits.setdefault(v,[]).append(b)
            break
        if (j>>21)&31==rd and op not in(36,32,34,40,48,50,54,58,62): break
res=[]
for v,refs in sorted(hits.items()):
    p=W(v-4)
    if p in(0x4e800020,0x4e800420,0) or (p>>26==18 and p&1==0):
        res.append(v); print(f'0x{v:08X} prev={p:08X} first={W(v):08X} refs={len(refs)} {hex(refs[0])}')
print(len(hits),'raw',len(res),'plausible',file=sys.stderr)
