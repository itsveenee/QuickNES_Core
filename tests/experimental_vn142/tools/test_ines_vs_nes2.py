#!/usr/bin/env python3
"""vN143: test real core NES 1.0 / NES 2.0 metadata routing and exclusions."""
from pathlib import Path
import ctypes as C,sys,json
from run_actual_quicknes_libretro import Headless,Game
root=Path(__file__).resolve().parents[1]
core=Path(sys.argv[1] if len(sys.argv)>1 else str(root/'build/quicknes_libretro.so'))
manifest=[]
def mutate(f,to_ines=False,mapper=None,submapper=0):
 b=bytearray(f.read_bytes())
 if mapper is None: mapper=((b[6]>>4)|(b[7]&0xf0)|((b[8]&15)<<8))
 assert mapper<=4095
 b[6]=(b[6]&0x0f)|((mapper&0x0f)<<4)
 b[7]=(b[7]&0x0f)|(mapper&0xf0)
 b[8]=(mapper>>8)&15 | (submapper<<4)
 if to_ines:
   assert mapper<256
   b[7]&=~0x0c # no NES2
   b[8:16]=bytes(8)
 else:b[7]=(b[7]&0xf3)|0x08
 return bytes(b)
M=[('Active_219_iNES',root/'smoke/Aurora_Active_Diagnostic_219.nes',True,None,'green'),
 ('Active_249_iNES',root/'smoke/Aurora_Active_Diagnostic_249.nes',True,None,'green'),
 ('Extended_219_iNES',root/'smoke/Aurora_Extended_Diagnostic_219.nes',True,None,'green'),
 ('Extended_249_iNES',root/'smoke/Aurora_Extended_Diagnostic_249.nes',True,None,'green'),
 ('Canonical_4_sub5_to_iNES4',root/'smoke/Aurora_Extended_Diagnostic_4_sub5.nes',True,None,'red'),
 ('Unsupported_mapper229_NES2',root/'smoke/Aurora_Active_Diagnostic_249.nes',False,229,'reject'),
 ('Unsupported_mapper259_NES2',root/'smoke/Aurora_Active_Diagnostic_249.nes',False,259,'reject'),
 ('Unsupported_mapper269_NES2',root/'smoke/Aurora_Active_Diagnostic_249.nes',False,269,'reject')]
h=Headless(core)
try:
 for name,f,ines,mapper,expected in M:
  dat=mutate(f,to_ines=ines,mapper=mapper)
  path=root/'smoke'/(name+'.nes');path.write_bytes(dat)
  if expected=='reject':
   buf=C.create_string_buffer(dat)
   g=Game(str(path).encode(),C.cast(buf,C.c_void_p),len(dat),None)
   ok=not h.lib.retro_load_game(C.byref(g))
   if not ok:h.lib.retro_unload_game()
   detail={'case':name,'pass':ok,'expected':'unsupported mapper rejected'}
  else:
   _,pal0,pal1,pc,sz=h.load_and_run(path,35)
   color=0x2a if expected=='green' else 0x16
   ok=(pal0==color and pal1==color and pc>=0x8000 and pc<0x9000)
   detail={'case':name,'pass':ok,'expected':hex(color),'actual':[hex(pal0),hex(pal1)],'pc':hex(pc),'mapr_size':sz}
  manifest.append(detail)
  print('PASS' if ok else 'FAIL',name,detail,flush=True)
finally:h.close()
(root/'RESULTS_headers.json').write_text(json.dumps(manifest,indent=2)+'\n')
assert len(manifest)==len(M) and all(x['pass'] for x in manifest)
print('PASS',len(manifest),'iNES/NES2 cross-header tests on actual QuickNES')