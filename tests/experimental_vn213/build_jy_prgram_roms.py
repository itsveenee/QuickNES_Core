#!/usr/bin/env python3
"""vN213: mapper209 NES2 volatile PRG-RAM $6000 visibility and negative."""
from pathlib import Path
import hashlib,json,sys
THIS=Path(__file__).resolve().parent;OUT=THIS/'smoke';OUT.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(THIS.parent/'experimental_vn163'))
from build_seven_new_roms import A,header,initialize
cases={}
for mapper in (35,209):
  for declared in (True,False):
    p=A(0x8000); initialize(p)
    p.sta(0x5a,0x6000)
    p.check(0x6000,0x5a)
    p.finish();prog=p.code()
    prg=bytearray(64*8192)
    for b in range(64):
      o=b*8192;prg[o:o+len(prog)]=prog
      for v in (0x1ffa,0x1ffc,0x1ffe):
        prg[o+v:o+v+2]=(0x8000).to_bytes(2,'little')
    h=bytearray(header(mapper,len(prg),0))
    # NES2 byte10, low nibble = volatile PRG RAM, 7 => 8 KiB
    h[10]=7 if declared else 0
    # Standard iNES mapper35 convention allows 8KiB even with no NES2 size.
    expected=(mapper==35 or declared)
    data=bytes(h)+bytes(prg)
    name='Aurora_vN213_{}_{}'.format(mapper,'PRGRAM' if declared else 'NO_PRGRAM')+'.nes'
    (OUT/name).write_bytes(data)
    cases[name]={'mapper':mapper,'declared_ram':declared,'expected':42 if expected else 22,
                 'sha256':hashlib.sha256(data).hexdigest()}
(OUT/'manifest.json').write_text(json.dumps(cases,indent=2)+'\n')
print('Generated',len(cases),'mapper35/209 volatile PRG RAM diagnostics')
