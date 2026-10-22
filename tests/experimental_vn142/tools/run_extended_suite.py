#!/usr/bin/env python3
"""Executable tests using the real QuickNES libretro core, with negative controls."""
from pathlib import Path
import hashlib,subprocess,sys,json,ctypes as C
from run_actual_quicknes_libretro import Headless
root=Path(__file__).resolve().parents[1]
core=Path(sys.argv[1] if len(sys.argv)>1 else str(root/'build'/'quicknes_libretro.so'))
assert core.is_file(),core
for script in ('build_extended_roms.py','build_irq_roms.py'):
 subprocess.run([sys.executable,str(root/'tools'/script)],check=True,stdout=subprocess.DEVNULL)
manifest=json.loads((root/'smoke'/'extended_manifest.json').read_text())
headless=Headless(core)
results=[]
try:
 for item in manifest:
  f=root/'smoke'/item['name']
  raw=f.read_bytes()
  for variant in ('ok','bad_prg','bad_chr'):
   if variant=='ok': path=f
   else:
    b=bytearray(raw)
    if variant=='bad_prg':
     # Corrupt an expected immediate on the FIRST PRG comparison in every
     # physical 8KiB bank; preserve vectors, opcode and all machine logic.
     k=b[16:16+0x400].index(0xc9) + 1
     for n in range(64):b[16+8192*n+k] ^= 0xff
    else:
     # Corrupt the CHR signature per physical 1KiB bank in both pixel bytes.
     base=16+64*8192
     for n in range(512):
      b[base+n*1024+0x80]^=0xff
      b[base+n*1024+0x81]^=0xff
    path=root/'smoke'/(f.stem+'_'+variant+'.nes');path.write_bytes(b)
   frames,bg0,bg1,pc,mapr=headless.load_and_run(path,35)
   expected=0x2a if variant=='ok' else 0x16
   expected_ext = 1 if '_219_ext_active' in f.name else 0 if '_219' in f.name else 7
   good=(bg0==expected and bg1==expected and 0x8000<=pc<0x9000 and mapr==(18 if '_219' in f.name else 16) and (variant!='ok' or headless.last_mapr[-1]==expected_ext))
   results.append({'case':f.stem,'variant':variant,'success':good,'palette':[bg0,bg1], 'PC':hex(pc),'MAPR':mapr,'ROM_SHA256':hashlib.sha256(path.read_bytes()).hexdigest()})
   print('PASS' if good else 'FAIL',f.stem,variant,'PPUR',hex(bg0),hex(bg1),'PC',hex(pc),'MAPR',mapr,flush=True)
finally:headless.close()
(root/'RESULTS_extended.json').write_text(json.dumps(results,indent=2)+'\n')
assert len(results)==12 and all(r['success'] for r in results)
print('PASS 12 extended mapper-219/249/4.5 REAL QuickNES tests including negative controls')