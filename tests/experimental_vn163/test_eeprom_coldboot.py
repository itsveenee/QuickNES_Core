#!/usr/bin/env python3
"""Run write ROM, save 512B, close, reopen read-only ROM, restore, read.
No mid-run output patching: reads are genuine serial $5200/$5000 protocol.
"""
from pathlib import Path
import ctypes as C,json,sys
from run_actual_quicknes_libretro import Headless,Game
ROOT=Path(__file__).parent;h=Headless(Path(sys.argv[1]));l=h.lib
l.retro_get_memory_size.argtypes=[C.c_uint];l.retro_get_memory_size.restype=C.c_size_t
l.retro_get_memory_data.argtypes=[C.c_uint];l.retro_get_memory_data.restype=C.c_void_p
l.retro_serialize_size.restype=C.c_size_t;l.retro_serialize.argtypes=[C.c_void_p,C.c_size_t];l.retro_serialize.restype=C.c_bool

def play(p,restore=None):
 raw=p.read_bytes();buf=C.create_string_buffer(raw);g=Game(str(p).encode(),C.cast(buf,C.c_void_p),len(raw),None)
 if not l.retro_load_game(C.byref(g)):raise RuntimeError('Load failed '+p.name)
 try:
  sz=l.retro_get_memory_size(0);ptr=l.retro_get_memory_data(0)
  assert sz==512 and ptr
  if restore is not None:
   assert len(restore)==512
   C.memmove(ptr,restore,512)
  for _ in range(45):l.retro_run()
  saved=C.string_at(ptr,512)
  n=l.retro_serialize_size();buf2=C.create_string_buffer(n);assert l.retro_serialize(buf2,n)
  data=buf2.raw
  def block(tag):
   i=data.index(tag);cnt=int.from_bytes(data[i+4:i+8],'little');return data[i+8:i+8+cnt]
  ppu=block(b'PPUR');cpu=block(b'CPUR')
  return dict(size=sz,EEPROM5=saved[5],EEPROM4=saved[4],palette=[ppu[12],ppu[13]],pc=hex(int.from_bytes(cpu[:2],'little'))),saved
 finally:l.retro_unload_game()
try:
 write_state,battery=play(ROOT/'smoke/Aurora_vN166_mapper558_EEPROM_EWEN_WRITE_READ_WRAM.nes')
 read_state,again=play(ROOT/'smoke/Aurora_vN167_mapper558_93C66_coldboot_READ_only.nes',battery)
 ok=(write_state['EEPROM5']==0xA5 and read_state['EEPROM5']==0xA5 and read_state['palette']==[42,42] and battery==again)
 info={'pass':ok,'write_then_unload':write_state,'reopen_with_512B_restore':read_state,'save_unchanged':battery==again}
 print(json.dumps(info,indent=2))
 (ROOT/'result_eeprom_coldboot.json').write_text(json.dumps(info,indent=2)+'\n')
 if not ok:sys.exit(1)
finally:h.close()