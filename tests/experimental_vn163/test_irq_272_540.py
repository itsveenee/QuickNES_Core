#!/usr/bin/env python3
"""Run 272 direct PAL and 540 CPU IRQ checks in the actual QuickNES core."""
from pathlib import Path
import argparse,json,sys
from run_actual_quicknes_libretro import Headless
BASE=Path(__file__).resolve().parent
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('core',type=Path);p=ap.parse_args()
 h=Headless(p.core);report=[]
 try:
  for name in sorted((BASE/'smoke').glob('Aurora_vN169_mapper*_IRQ_*.nes')):
   _,bg0,bg1,pc,sz=h.load_and_run(name,frames=35)
   want=22 if 'negative' in name.name else 42
   passed=(bg0,bg1)==(want,want) and sz in (16,271)
   entry=dict(name=name.name,pass_=passed,ppu_palette=[bg0,bg1],pc=hex(pc),mapr_bytes=sz)
   report.append(entry)
   print('PASS' if passed else 'FAIL',entry,flush=True)
 finally:h.close()
 (BASE/'result_irq_272_540.json').write_text(json.dumps(report,indent=2)+'\n')
 if len(report)!=4 or not all(x['pass_'] for x in report):sys.exit(1)