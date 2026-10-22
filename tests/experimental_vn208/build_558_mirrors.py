#!/usr/bin/env python3
"""vN208: original NES2 mapper558 mirrored register readback (+ negative)."""
from pathlib import Path
import hashlib, json, sys
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parent/'experimental_vn163'))
from build_seven_new_roms import A,header,initialize
OUT=HERE/'smoke'
OUT.mkdir(parents=True,exist_ok=True)
manifest={}
for positive in (True,False):
    p=A(0x8000)
    initialize(p)
    # $5600 must alias serial register $5200; without alias read stays zero.
    p.sta(4 if positive else 0,0x5600)
    p.check(0x5000,4)
    # Set $5300 physical bit 1 through mirrored $5700 (raw D0/D1 crossed).
    p.sta(1,0x5700)
    p.sta(1,0x5400)
    p.sta(2,0x5500)
    p.check(0x5000,4)
    p.finish()
    program=p.code()
    prg=bytearray(16*32768)
    for i in range(16):
        pos=i*32768
        prg[pos:pos+len(program)]=program
        for vector in (0x7ffa,0x7ffc,0x7ffe):
            prg[pos+vector:pos+vector+2]=(0x8000).to_bytes(2,'little')
    blob=header(558,len(prg),8192)+prg+bytes(8192)
    name='Aurora_vN208_mapper558_register_mirror_'+('positive' if positive else 'negative')+'.nes'
    (OUT/name).write_bytes(blob)
    manifest[name]={'mapper':558,'positive':positive,'expected_palette':42 if positive else 22,'sha256':hashlib.sha256(blob).hexdigest()}
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Created mapper558 mirror positive+negative with hashes')
