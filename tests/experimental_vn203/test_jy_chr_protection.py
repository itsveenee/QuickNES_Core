#!/usr/bin/env python3
"""Exercise actual QuickNES libretro core with positive and intentionally failing ROMs."""
from pathlib import Path
import argparse,importlib.util,json
ROOT=Path(__file__).resolve().parent
# In CI the JYASIC runner is already published as part of vN154.
paths=[ROOT/'../experimental_vn151/tools/run_actual_quicknes_libretro.py',ROOT/'SNESticleAurora_QuickNES_vN160_JYASIC_35_209/tools/run_actual_quicknes_libretro.py']
path=next((p for p in paths if p.exists()),None)
if path is None:raise RuntimeError('Missing existing native QuickNES test harness')
spec=importlib.util.spec_from_file_location('quicknes_native_jy_runner',path)
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
p=argparse.ArgumentParser();p.add_argument('core',type=Path);p.add_argument('--strict',action='store_true');args=p.parse_args()
runner=module.Headless(args.core.resolve());out={};ok=True
try:
 for mapper in (35,209):
  for negative in (False,True):
   name=f'Aurora_JYCHR_{mapper}_'+('negative' if negative else 'positive')+'.nes'
   _,p0,p1,pc,mb=runner.load_and_run(ROOT/'smoke'/name,frames=25)
   expected=0x16 if negative else 0x2a
   passed=p0==expected and p1==expected and mb==50 and 0x8000<=pc<0x8200
   out[name]={'palette':[p0,p1],'pc':pc,'mapper_state_bytes':mb,'expected':expected,'passed':passed}
   print(name,'PASS' if passed else 'FAIL','actual',hex(p0),hex(p1),'expected',hex(expected),flush=True)
   ok &=passed
finally:runner.close()
(ROOT/'results_jy_chr_protection.json').write_text(json.dumps(out,indent=2)+'\n')
print('OVERALL:',ok,flush=True)
if args.strict and not ok:raise SystemExit(1)