#!/usr/bin/env python3
"""Original NES2 558 CHR-RAM aliasing and 1bpp background diagnostics."""
from pathlib import Path
import hashlib, json, sys
THIS=Path(__file__).resolve().parent
sys.path.insert(0,str(THIS.parent/'experimental_vn163'))
# Fallback for executing this script outside GitHub checkout
try:
    from build_seven_new_roms import A, header, initialize
except ImportError:
    sys.path.insert(0,'/mnt/data/vN161_vN175')
    from build_seven_new_roms import A, header, initialize
OUT=THIS/'smoke';OUT.mkdir(exist_ok=True)

def ptr(p,a):
    p.sta((a>>8)&255,0x2006);p.sta(a&255,0x2006)
def block(p,a,vals):
    ptr(p,a)
    for v in vals:p.sta(v,0x2007)
def read_check(p,a,val):
    ptr(p,a)
    p.ab(0xad,0x2007);p.ab(0xad,0x2007)
    good='chr_'+str(p.n);p.n+=1
    p.imm(0xc9,val);p.br(0xf0,good);p.jmp('fail');p.mark(good)

def rom(mode,negative=False):
    p=A(0x8000);initialize(p)
    p.sta(6,0x5300)   # no D0/D1 bank inversion, boot bank 0
    p.sta(0x80 if mode else 0,0x5000)
    p.sta(0x00,0x2000);p.sta(0x00,0x2001)
    block(p,0x0000,[0xff]*8);block(p,0x0008,[0]*8)
    block(p,0x1000,[0]*8);block(p,0x1008,[0xff]*8)
    if negative is not None:
        read_check(p,0x0008,0x00)
        read_check(p,0x1008,0x66 if negative else 0xff)
    block(p,0x2000,[0]*96)
    block(p,0x23c0,[0]*64)
    # background color palette selection; 1bpp should alternate 0 and 3 by tile column
    block(p,0x3f00,[0x0f,0x16,0x2a,0x30])
    ptr(p,0x2000)
    p.sta(0,0x2005);p.sta(0,0x2005)
    p.sta(0x08,0x2001)  # BG ON (including leftmost 8 pixels: bit1 not essential)
    p.mark('loop');p.jmp('loop')
    p.mark('fail');block(p,0x3f00,[0x16,0x16]);p.sta(0x08,0x2001);p.mark('bad');p.jmp('bad')
    code=p.code(); assert len(code)<0x2000,(len(code),mode)
    prg=bytearray(16*32768)
    for bank in range(16):
        at=bank*32768
        prg[at:at+len(code)]=code
        for vec in (0x7ffa,0x7ffc,0x7ffe):prg[at+vec:at+vec+2]=(0x8000).to_bytes(2,'little')
    return header(558,len(prg),0)+prg

manifest={}
for key,mode,negative in [('normal_pass',False,False),('normal_negative',False,True),('1bpp_visual',True,None),('normal_visual',False,None)]:
    file=OUT/('Mapper558_'+key+'.nes');data=rom(mode,negative)
    file.write_bytes(data);manifest[file.name]={'mapper':558,'mode':int(mode),'negative':negative,'sha256':hashlib.sha256(data).hexdigest(),'length':len(data)}
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Generated',len(manifest),'original mapper558 1bpp/CHR ROMs')