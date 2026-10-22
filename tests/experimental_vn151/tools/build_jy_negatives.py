#!/usr/bin/env python3
"""Make deliberate failing variants of our own test ROMs; no third-party data."""
from pathlib import Path
import hashlib,json
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'smoke'/'negative';OUT.mkdir(parents=True,exist_ok=True)
SRC=ROOT/'smoke'
BANK=8192
PRG_START=16
CHR_START=PRG_START+64*BANK
entries={}
for mid in (35,209):
    base=bytearray((SRC/f'Aurora_JYASIC_{mid}_PRG_CHR_ALU.nes').read_bytes())
    assert len(base)==16+64*BANK+512*1024
    # PRG bank $12: this physical marker is checked at $B000.
    bad=bytearray(base);bad[PRG_START+0x12*BANK+0x1000]=0x00
    path=OUT/f'Aurora_JYASIC_{mid}_bad_PRG.nes';path.write_bytes(bad)
    entries[path.name]={'type':'PRG','sha256':hashlib.sha256(bad).hexdigest()}
    # CHR bank 24 marker: first CHR comparison via $2007.
    bad=bytearray(base)
    bad[CHR_START+24*1024+0x80]=0
    bad[CHR_START+24*1024+0x81]=0
    path=OUT/f'Aurora_JYASIC_{mid}_bad_CHR.nes';path.write_bytes(bad)
    entries[path.name]={'type':'CHR','sha256':hashlib.sha256(bad).hexdigest()}
    # Remove the single post-enable zero-page write, leaving the remaining
    # opcode timing and IRQ configuration unchanged.
    bad=bytearray((SRC/f'Aurora_JYIRQ_{mid}_write_low.nes').read_bytes())
    needle=bytes((0xa9,0x55,0x8d,0x11,0x00))
    fixed=bytes((0xa9,0x55,0xea,0xea,0xea))
    count=0
    for b in range(64):
        off=PRG_START+b*BANK
        page=bytes(bad[off:off+4096])
        if page.count(needle)!=1:raise ValueError((mid,b,page.count(needle)))
        bad[off:off+4096]=page.replace(needle,fixed,1);count+=1
    assert count==64
    path=OUT/f'Aurora_JYIRQ_{mid}_bad_missing_low_write.nes';path.write_bytes(bad)
    entries[path.name]={'type':'IRQ low-write','sha256':hashlib.sha256(bad).hexdigest()}
(OUT/'manifest.json').write_text(json.dumps(entries,indent=2)+'\n')
print('Generated',len(entries),'negative controls for 35/209, each expected to fail to red')