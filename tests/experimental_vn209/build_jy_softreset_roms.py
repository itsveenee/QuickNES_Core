#!/usr/bin/env python3
"""vN209: original mapper35/209 JYASIC soft-reset persistence CPU test ROMs.

The first program execution writes ALU registers and a CPU RAM phase marker,
then parks in a loop. retro_reset() must preserve JY ASIC registers and CPU
RAM; the second execution verifies ALU multiply + adder, reporting PPU color.
"""
from pathlib import Path
import hashlib,json,sys
HERE=Path(__file__).resolve().parent
OUT=HERE/'smoke';OUT.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(HERE.parent/'experimental_vn163'))
from build_seven_new_roms import A,header
manifest={}
for mapper in (35,209):
  for positive in (True,False):
    p=A(0x8000)
    p.emit(0x78,0xd8)
    p.sta(0,0x2000);p.sta(0,0x2001)
    p.ab(0xad,0x0020) # RAM flag survives a soft reset
    p.imm(0xc9,0x5a)
    p.br(0xf0,'phase2')
    p.sta(0x5a,0x0020)
    if positive:
      p.sta(11,0x5800)
      p.sta(13,0x5801)
      p.sta(0x19,0x5802)
    p.mark('idle');p.jmp('idle')
    p.mark('phase2')
    p.check(0x5800,143)
    p.check(0x5801,0)
    p.check(0x5802,0x19)
    p.finish()
    code=p.code()
    prg=bytearray(64*8192)
    for page in range(64):
      off=page*8192
      prg[off:off+len(code)]=code
      for vec in (0x1ffa,0x1ffc,0x1ffe):
        prg[off+vec:off+vec+2]=(0x8000).to_bytes(2,'little')
    blob=header(mapper,len(prg),0)+prg
    name=f'Aurora_vN209_JYSOFT_{mapper}_'+('positive' if positive else 'negative')+'.nes'
    (OUT/name).write_bytes(blob)
    manifest[name]={'mapper':mapper,'positive':positive,'expected_palette':42 if positive else 22,'sha256':hashlib.sha256(blob).hexdigest()}
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('vN209 generated',len(manifest),'positive/negative JYASIC soft-reset ROMs')
