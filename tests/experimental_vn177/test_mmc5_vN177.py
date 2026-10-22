#!/usr/bin/env python3
"""Run six original MMC5 ExRAM tests in a complete libretro QuickNES core."""
from pathlib import Path
import sys, json, argparse
p=argparse.ArgumentParser();p.add_argument('core');p.add_argument('--smoke',type=Path,default=Path(__file__).parent/'smoke');p.add_argument('--strict',action='store_true');o=p.parse_args()
sys.path.insert(0,str(Path(__file__).resolve().parent))
from run_actual_quicknes_libretro import Headless
manifest=json.loads((o.smoke/'manifest.json').read_text())
h=Headless(Path(o.core));results=[]
try:
 for name,meta in sorted(manifest.items()):
  try:
   _,bg0,bg1,pc,_=h.load_and_run(o.smoke/name,frames=25)
   expected=0x2a if meta['expect']=='green' else 0x16
   ok=bg0==bg1==expected
   results.append({'rom':name,'passed':ok,'ppu':[bg0,bg1],'pc':hex(pc),'expected':expected})
   print(('PASS' if ok else 'FAIL'),name,'expected',hex(expected),'got',hex(bg0),hex(bg1),flush=True)
  except Exception as exc:
   results.append({'rom':name,'passed':False,'exception':str(exc)});print('FAIL',name,exc,flush=True)
finally:h.close()
(o.smoke.parent/'result_mmc5_exram.json').write_text(json.dumps(results,indent=2)+'\n')
if o.strict and not all(x['passed'] for x in results):sys.exit(1)