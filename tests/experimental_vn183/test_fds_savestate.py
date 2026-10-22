#!/usr/bin/env python3
"""vN188: verify FDS audio frame-exact replay after libretro savestate restore."""
import ctypes as C,sys,json,hashlib
from pathlib import Path
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parent/'experimental_vn163'))
from run_actual_quicknes_libretro import Headless,Game,BATCH

def main():
 if len(sys.argv)!=2:raise SystemExit('Usage: test_fds_savestate.py core.so')
 h=Headless(Path(sys.argv[1]).resolve());l=h.lib
 arr=[]
 def cb(ptr,n):
  if ptr and n:
   p=C.cast(ptr,C.POINTER(C.c_int16));arr.extend(int(p[i]) for i in range(int(n)*2))
  return n
 cb_obj=BATCH(cb);l.retro_set_audio_sample_batch(cb_obj)
 path=HERE/'smoke'/'Mapper540_FDS_active.nes'
 data=path.read_bytes();buf=C.create_string_buffer(data)
 game=Game(str(path).encode(),C.cast(buf,C.c_void_p),len(data),None)
 assert l.retro_load_game(C.byref(game))
 l.retro_serialize_size.restype=C.c_size_t
 l.retro_serialize.argtypes=[C.c_void_p,C.c_size_t];l.retro_serialize.restype=C.c_bool
 l.retro_unserialize.argtypes=[C.c_void_p,C.c_size_t];l.retro_unserialize.restype=C.c_bool
 try:
  for _ in range(10):l.retro_run()
  sz=l.retro_serialize_size();state=C.create_string_buffer(sz);assert l.retro_serialize(state,sz)
  arr.clear()
  for _ in range(6):l.retro_run()
  first=list(arr)
  arr.clear();assert l.retro_unserialize(state,sz)
  for _ in range(6):l.retro_run()
  second=list(arr)
  def digest(values):
   b=C.create_string_buffer(len(values)*2)
   ptr=C.cast(b,C.POINTER(C.c_int16))
   for i,x in enumerate(values):ptr[i]=x
   return hashlib.sha256(b.raw).hexdigest()
  m={'samples_before':len(first),'samples_after':len(second),'energy_before':sum(v*v for v in first),'energy_after':sum(v*v for v in second),'hash_before':digest(first),'hash_after':digest(second),'pass':first==second and len(first)>6000 and sum(v*v for v in first)>10000}
  (HERE/'result_fds_savestate.json').write_text(json.dumps(m,indent=2)+'\n')
  print(json.dumps(m,indent=2));assert m['pass'],'FDS save/restore PCM mismatch'
 finally:
  l.retro_unload_game();h.close()
if __name__=='__main__':main()