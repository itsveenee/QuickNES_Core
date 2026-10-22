#!/usr/bin/env python3
"""vN189: additional sound ROMs, varying FDS fixed volume and period."""
import sys,hashlib,json
from pathlib import Path
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parent/'experimental_vn163'))
from build_seven_new_roms import A,header,initialize
OUT=HERE/'smoke';OUT.mkdir(parents=True,exist_ok=True)
def build(volume,period):
 p=A(0xe000);initialize(p)
 p.sta(0x80,0x4089)
 for i in range(64):p.sta(0x3f if i&16 else 0,0x4040+i)
 p.sta(0x80+(volume&0x3f),0x4080)
 p.sta(period&0xff,0x4082);p.sta((period>>8)&15,0x4083)
 p.sta(0,0x4089);p.mark('end');p.jmp('end')
 code=p.code();prg=bytearray(64*8192);prg[-8192:-8192+len(code)]=code
 for addr in (0x1ffa,0x1ffc,0x1ffe):
  idx=len(prg)-8192+addr;prg[idx:idx+2]=(0xe000).to_bytes(2,'little')
 return header(540,len(prg),8192)+prg+bytes(8192)
variants={'vol08':(8,0x80),'vol16':(16,0x80),'vol31':(31,0x80),'pitch100':(31,0x100)}
meta={}
for name,(volume,period) in variants.items():
 data=build(volume,period);filename='Mapper540_FDS_'+name+'.nes'
 (OUT/filename).write_bytes(data)
 meta[name]={'volume':volume,'period':period,'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data)}
(OUT/'matrix_manifest.json').write_text(json.dumps(meta,indent=2)+'\n')
print('Wrote',len(meta),'FDS volume/frequency ROMs')