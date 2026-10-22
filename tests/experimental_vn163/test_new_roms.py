#!/usr/bin/env python3
"""Execute 272,540,558 ROMs inside actual compiled Linux QuickNES; report results."""
from pathlib import Path
import argparse,sys,json
THIS=Path(__file__).resolve().parent
sys.path.insert(0,str(THIS))
from run_actual_quicknes_libretro import Headless

def main():
 p=argparse.ArgumentParser();p.add_argument('core',type=Path);p.add_argument('--strict',action='store_true');opts=p.parse_args()
 m=Headless(opts.core);output=[]
 try:
  for name,meta in json.loads((THIS/'smoke/manifest.json').read_text()).items():
   f=THIS/'smoke'/name
   try:
    _,bg0,bg1,pc,mapr=m.load_and_run(f,frames=35)
    expected=0x2a if meta['expect']=='green' else 0x16
    passed=(bg0,bg1)==(expected,expected) and (0x8000<=pc<=0xffff)
    output.append(dict(name=name,ok=passed,expected=hex(expected),ppu=[hex(bg0),hex(bg1)],pc=hex(pc),mapr=mapr))
    print('PASS' if passed else 'FAIL', name, 'pal=',hex(bg0),hex(bg1),'pc=',hex(pc),'state=',mapr,flush=True)
   except Exception as e:
    output.append(dict(name=name,ok=False,error=str(e)))
    print('EXCEPTION',name,repr(e),flush=True)
 finally:m.close()
 (THIS/'results_163.json').write_text(json.dumps(output,indent=2)+'\n')
 if opts.strict and not all(x['ok'] for x in output):sys.exit(1)
if __name__=='__main__':main()