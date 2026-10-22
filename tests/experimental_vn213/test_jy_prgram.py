#!/usr/bin/env python3
"""Exercise mapper35/209 volatile PRG-RAM header with actual QuickNES core."""
from pathlib import Path
import sys,json
THIS=Path(__file__).resolve().parent
sys.path.insert(0,str(THIS.parent/'experimental_vn163'))
from run_actual_quicknes_libretro import Headless
m=Headless(Path(sys.argv[1]).resolve())
manifest=json.loads((THIS/'smoke/manifest.json').read_text())
try:
  for name,meta in manifest.items():
    _,c0,c1,pc,sz=m.load_and_run(THIS/'smoke'/name,30)
    ok=(c0,c1)==(meta['expected'],meta['expected']) and sz==50
    print(('PASS' if ok else 'FAIL'),name,'palette',c0,c1,'mapper bytes',sz,flush=True)
    if not ok:raise SystemExit(1)
finally:m.close()
print('PASS mapper35/209 NES2 volatile PRG RAM visibility diagnostics')
