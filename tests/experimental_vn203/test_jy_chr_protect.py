#!/usr/bin/env python3
"""Load JYASIC CHR-RAM test ROMs in the complete QuickNES libretro core."""
import argparse,json,sys
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('core',type=Path);p.add_argument('--strict',action='store_true');a=p.parse_args()
sys.path.insert(0,str(Path(__file__).resolve().parent.parent/'experimental_vn151'/'tools'))
# Standalone local package fallback, never an emulation substitute.
if not (Path(sys.path[0])/'run_actual_quicknes_libretro.py').exists():
    sys.path.insert(0,str(Path('/mnt/data/SNESticleAurora_QuickNES_vN160_JYASIC_35_209/tools')))
from run_actual_quicknes_libretro import Headless
smoke=Path(__file__).resolve().parent.parent/'smoke'
items=json.loads((smoke/'manifest.json').read_text())
m=Headless(a.core.resolve())
records=[]
try:
    for name,info in items.items():
        _,c1,c2,pc,state_len=m.load_and_run(smoke/name,35)
        expected=int(info['expected_palette'],16)
        ok=c1==expected and c2==expected and state_len==50 and 0x8020<pc<0x9000
        records.append({'rom':name,'palette':[c1,c2],'expected':expected,'pc':pc,'mapper_state_bytes':state_len,'pass':ok})
        print(('PASS' if ok else 'FAIL'),name,'palette',hex(c1),hex(c2),'PC',hex(pc),'MAPR',state_len,flush=True)
finally:m.close()
result={'passed':sum(int(x['pass']) for x in records),'count':len(records),'records':records,'coverage':['CHR-RAM initial normal write','CHR-RAM protected write','CHR-RAM re-enabled write','real CPU $2007 path','negative controls','savestate roundtrip']}
(smoke.parent/'result_jy_chr_protect.json').write_text(json.dumps(result,indent=2)+'\n')
if a.strict and result['passed']!=result['count']:sys.exit(1)