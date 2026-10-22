#!/usr/bin/env python3
"""vN207: coldboot EEPROM readback, with physical $5300 D0/D1 swap."""
import sys,json,hashlib
from pathlib import Path
ROOT=Path(__file__).parent;sys.path.insert(0,str(ROOT))
from build_seven_new_roms import A,header,initialize,OUT
p=A(0x8000);initialize(p);p.sta(1,0x5300)
def bit(v):p.sta(4|v,0x5200);p.sta(6|v,0x5200)
p.sta(0,0x5200);bit(1);bit(1);bit(0)
for i in range(8,-1,-1):bit((5>>i)&1)
# 0xA5 = 1010 0101, verify the high four bits to test EEPROM restore.
for v in [4,0,4,0]:
 bit(0);p.check(0x5000,v)
p.finish();code=p.code();prg=bytearray(16*32768)
for b in range(16):
 off=b*32768;prg[off:off+len(code)]=code
 for vec in (0x7ffa,0x7ffc,0x7ffe):prg[off+vec:off+vec+2]=(0x8000).to_bytes(2,'little')
h=bytearray(header(558,len(prg),8192));h[10]=0x30
rom=bytes(h)+prg+bytes(8192)
fn=OUT/'Aurora_vN167_mapper558_93C66_coldboot_READ_only.nes';fn.write_bytes(rom)
print(fn.name,len(rom),'program',len(code),'sha256',hashlib.sha256(rom).hexdigest())