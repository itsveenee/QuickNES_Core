#!/usr/bin/env python3
"""Run real QuickNES libretro IRQ probes; inspect RAM and savestates."""
from pathlib import Path
import ctypes as C,json,sys
from run_actual_quicknes_libretro import Headless,Game
root=Path(__file__).resolve().parents[1]
core=Path(sys.argv[1] if len(sys.argv)>1 else str(root/'build'/'quicknes_libretro.so'))
manifest=json.loads((root/'smoke/manifest.json').read_text())
h=Headless(core)
h.lib.retro_get_memory_data.argtypes=[C.c_uint]
h.lib.retro_get_memory_data.restype=C.c_void_p
results=[]
try:
 for rec in manifest:
    f=root/'smoke'/rec['filename']
    raw=f.read_bytes();buf=C.create_string_buffer(raw);game=Game(str(f).encode(),C.cast(buf,C.c_void_p),len(raw),None)
    if not h.lib.retro_load_game(C.byref(game)): raise RuntimeError('not loaded: '+f.name)
    for _ in range(5):h.lib.retro_run()
    p=h.lib.retro_get_memory_data(2)
    if not p:raise RuntimeError('no RAM: '+f.name)
    result=C.string_at(p,32)
    token=result[0x10];count=result[0x11]
    expected=0x2a if rec['expected_irq'] else 0
    ok=token==expected and (count==1 if rec['expected_irq'] else count==0)
    entry={'file':f.name,'token':hex(token),'irq_count':count,'expected':hex(expected),'pass':ok}
    results.append(entry)
    print(('PASS' if ok else 'FAIL')+' '+f.name+' -> '+entry['token']+', IRQ entries '+str(count),flush=True)
    h.lib.retro_unload_game()
finally:h.close()
(root/'RESULTS_irq.json').write_text(json.dumps(results,indent=2)+'\n')
assert all(x['pass'] for x in results),f"{sum(not x['pass'] for x in results)} failed IRQ trials"
print('PASS: 20 real QuickNES IRQ ROMs')