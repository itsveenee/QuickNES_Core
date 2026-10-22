#!/usr/bin/env python3
"""vN184: two original mapper 540 FDS ROMs; no commercial data."""
import hashlib, json, sys
from pathlib import Path
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parent/'experimental_vn163'))
from build_seven_new_roms import A, header, initialize
OUT=HERE/'smoke'; OUT.mkdir(parents=True,exist_ok=True)
def rom(active):
    p=A(0xe000);initialize(p)
    p.sta(0x80,0x4089) # enable wavetable writes
    for i in range(64):
        p.sta(0x3f if active and i&16 else 0,0x4040+i)
    p.sta(0x9f if active else 0x80,0x4080) # constant volume 31 or 0
    p.sta(0x80,0x4082)
    p.sta(0,0x4083)
    p.sta(0,0x4089) # enable DAC
    p.mark('end');p.jmp('end')
    code=p.code()
    prg=bytearray(64*8192)
    prg[-8192:-8192+len(code)]=code
    for v in (0x1ffa,0x1ffc,0x1ffe):
        idx=len(prg)-8192+v
        prg[idx:idx+2]=(0xe000).to_bytes(2,'little')
    return header(540,len(prg),8192)+prg+bytes(8192)
manifest={}
for active in [False,True]:
    mode='active' if active else 'silent'
    path=OUT/('Mapper540_FDS_'+mode+'.nes');data=rom(active);path.write_bytes(data)
    manifest[mode]={'path':path.name,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Two FDS diagnostic ROMs generated:',manifest)