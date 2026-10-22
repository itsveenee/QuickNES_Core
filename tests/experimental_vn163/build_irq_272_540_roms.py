#!/usr/bin/env python3
"""vN168-169: native IRQ delivery/ack diagnostics for 272 PAL and 540 CPU IRQ."""
from pathlib import Path
import sys,json,hashlib
sys.path.insert(0,str(Path(__file__).parent))
from build_seven_new_roms import A,header,initialize,OUT

def rom(mapper,negative):
 p=A(0xe000);initialize(p)
 p.sta(0,0xf0)
 if mapper==272:
  p.sta(0,0xc004)
  if not negative:p.sta(0,0x800c)
 else:
  p.sta(0x80,0xc000);p.sta(0,c:=0xc001)
  if not negative:p.sta(1,0xc002)
 p.emit(0x58) # CLI
 for _ in range(95):p.emit(0xea) # advance CPU time and permit IRQ
 p.emit(0x78) # SEI
 p.check(0xf0,1)
 p.finish();code=p.code()
 banks=16 if mapper==272 else 64;prg=bytearray(banks*8192)
 for b in range(banks):prg[b*8192+2]=b
 prg[-8192:-8192+len(code)]=code
 isr_off=0x300
 # Increment zero-page marker; acknowledge IRQ, then RTI.
 handler=bytes([0xe6,0xf0,0xa9,0,0x8d,0x04 if mapper==272 else 0x02,0xc0,0x40])
 prg[len(prg)-8192+isr_off:len(prg)-8192+isr_off+len(handler)]=handler
 for vec,target in ((0x1ffa,0xe000),(0x1ffc,0xe000),(0x1ffe,0xe300)):
  base=len(prg)-8192+vec;prg[base:base+2]=target.to_bytes(2,'little')
 chr=bytes(8192)
 return header(mapper,len(prg),len(chr))+prg+chr
result=[]
for mapper in (272,540):
 for negative in (False,True):
  fname=f'Aurora_vN169_mapper{mapper}_IRQ_'+('negative' if negative else 'positive')+'.nes'
  data=rom(mapper,negative);(OUT/fname).write_bytes(data)
  result.append({'file':fname,'mapper':mapper,'negative':negative,'sha256':hashlib.sha256(data).hexdigest()})
(OUT/'irq_272_540_manifest.json').write_text(json.dumps(result,indent=2)+'\n')
print('Generated',len(result),'original IRQ ROMs')