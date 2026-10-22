#!/usr/bin/env python3
"""Check 558 mirrored CPU register aliases inside actual QuickNES libretro."""
from pathlib import Path
import sys,json
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT.parent/'experimental_vn163'))
from run_actual_quicknes_libretro import Headless
items=json.loads((ROOT/'smoke/manifest.json').read_text())
core=Headless(Path(sys.argv[1]).resolve())
try:
  for name,item in items.items():
    _,c0,c1,pc,mapr=core.load_and_run(ROOT/'smoke'/name,frames=30)
    want=item['expected_palette']
    ok=(c0,c1)==(want,want) and mapr==19 and 0x8000<=pc<=0xffff
    print(('PASS' if ok else 'FAIL'),name,'PPUR',c0,c1,'MAPR',mapr,flush=True)
    if not ok:raise SystemExit(1)
finally:core.close()
print('PASS mapper558 actual CPU $5600 mirror, control negative and savestate')
