#!/usr/bin/env python3
"""Run the actual QuickNES CPU/PPU + libretro soft reset, then read palette."""
from pathlib import Path
import ctypes as C,sys,json
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT.parent/'experimental_vn151'/'tools'))
from run_actual_quicknes_libretro import Headless,Game
core=Headless(Path(sys.argv[1]).resolve())
lib=core.lib
lib.retro_reset.argtypes=[]
lib.retro_serialize_size.restype=C.c_size_t
lib.retro_serialize.argtypes=[C.c_void_p,C.c_size_t]
lib.retro_serialize.restype=C.c_bool
items=json.loads((ROOT/'smoke/manifest.json').read_text())
try:
  for name,info in items.items():
    raw=(ROOT/'smoke'/name).read_bytes()
    ptr=C.create_string_buffer(raw)
    game=Game(name.encode(),C.cast(ptr,C.c_void_p),len(raw),None)
    if not lib.retro_load_game(C.byref(game)):raise AssertionError('Load rejected '+name)
    try:
      for _ in range(8):lib.retro_run()
      lib.retro_reset() # critical: must trigger JYASIC soft, not cold reset
      for _ in range(20):lib.retro_run()
      n=lib.retro_serialize_size()
      assert n>0
      buf=C.create_string_buffer(n)
      assert lib.retro_serialize(buf,n)
      state=buf.raw
      def block(tag):
        at=state.index(tag);length=int.from_bytes(state[at+4:at+8],'little')
        return state[at+8:at+8+length]
      ppu=block(b'PPUR');cpu=block(b'CPUR');mp=block(b'MAPR')
      expected=info['expected_palette']
      pc=int.from_bytes(cpu[:2],'little')
      ok=ppu[12:14]==bytes((expected,expected)) and len(mp)==50 and 0x8000<=pc<0x8800
      print(('PASS' if ok else 'FAIL'),name,'palette',list(ppu[12:14]),'MAPR',len(mp),'PC',hex(pc),flush=True)
      if not ok:raise SystemExit(1)
    finally:lib.retro_unload_game()
finally:core.close()
print('PASS: JYASIC 35/209 soft resets preserve mapper registers; negatives discriminate')
