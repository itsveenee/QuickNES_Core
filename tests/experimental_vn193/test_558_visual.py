#!/usr/bin/env python3
"""Run mapper558 CHR and 1bpp video ROMs in the actual QuickNES libretro core."""
import ctypes as C, argparse, json, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent
class Game(C.Structure):
    _fields_=[('path',C.c_char_p),('data',C.c_void_p),('size',C.c_size_t),('meta',C.c_char_p)]
ENV=C.CFUNCTYPE(C.c_bool,C.c_uint,C.c_void_p)
VIDEO=C.CFUNCTYPE(None,C.c_void_p,C.c_uint,C.c_uint,C.c_size_t)
AUDIO=C.CFUNCTYPE(None,C.c_int16,C.c_int16)
BATCH=C.CFUNCTYPE(C.c_size_t,C.c_void_p,C.c_size_t)
POLL=C.CFUNCTYPE(None)
STATE=C.CFUNCTYPE(C.c_int16,C.c_uint,C.c_uint,C.c_uint,C.c_uint)
class Core:
    def __init__(self,path):
        lib=C.CDLL(str(path));self.lib=lib
        self.videos=[]
        def env(cmd,p):
            return cmd==10 and bool(p) and C.cast(p,C.POINTER(C.c_int)).contents.value==2
        def video(ptr,w,h,pitch):
            if not ptr or not w or not h:return
            # Sample fixed coordinates from separate background tiles, excluding border.
            xy=[(24,4),(32,4),(40,4),(48,4),(24,12),(32,12),(40,12),(48,12)]
            self.videos.append([int.from_bytes(C.string_at(int(ptr)+y*pitch+x*2,2),'little') if x<w and y<h else 0 for x,y in xy])
        self.cb=[ENV(env),VIDEO(video),AUDIO(lambda l,r:None),BATCH(lambda p,n:n),POLL(lambda:None),STATE(lambda a,b,c,d:0)]
        for fn,cb in zip(('retro_set_environment','retro_set_video_refresh','retro_set_audio_sample','retro_set_audio_sample_batch','retro_set_input_poll','retro_set_input_state'),self.cb):
            f=getattr(lib,fn);f.argtypes=[type(cb)];f(cb)
        lib.retro_load_game.argtypes=[C.POINTER(Game)];lib.retro_load_game.restype=C.c_bool
        lib.retro_serialize_size.restype=C.c_size_t
        lib.retro_serialize.argtypes=[C.c_void_p,C.c_size_t];lib.retro_serialize.restype=C.c_bool
        lib.retro_unserialize.argtypes=[C.c_void_p,C.c_size_t];lib.retro_unserialize.restype=C.c_bool
        lib.retro_init()
    def run(self,rom,frames=12):
        data=rom.read_bytes();blob=C.create_string_buffer(data);info=Game(str(rom).encode(),C.cast(blob,C.c_void_p),len(data),None)
        self.videos=[]
        if not self.lib.retro_load_game(C.byref(info)):raise RuntimeError('load failed: '+rom.name)
        try:
            for _ in range(frames):self.lib.retro_run()
            if not self.videos:raise RuntimeError('No video frames')
            n=self.lib.retro_serialize_size();b=C.create_string_buffer(n)
            if not self.lib.retro_serialize(b,n):raise RuntimeError('serialize failed')
            state=b.raw
            def tag(key):
                i=state.find(key)
                if i<0:raise RuntimeError('missing '+repr(key))
                k=int.from_bytes(state[i+4:i+8],'little');return state[i+8:i+8+k]
            ppu=tag(b'PPUR');cpu=tag(b'CPUR');
            # vN196: video output must reproduce after real libretro savestate restore.
            for _ in range(2):self.lib.retro_run()
            before=self.videos[-1]
            saved=C.create_string_buffer(state)
            if not self.lib.retro_unserialize(saved,n):raise RuntimeError('unserialize failed')
            for _ in range(2):self.lib.retro_run()
            if self.videos[-1]!=before:raise RuntimeError('Video differs after restoring mapper558 savestate')
            return {'samples':self.videos[-1],'pal':[ppu[12],ppu[13]],'pc':int.from_bytes(cpu[:2],'little'),'state_ok':True}
        finally:self.lib.retro_unload_game()
    def close(self):self.lib.retro_deinit()
def main():
    p=argparse.ArgumentParser();p.add_argument('core',type=Path);p.add_argument('--strict',action='store_true');o=p.parse_args()
    core=Core(o.core.resolve());out={}
    try:
        for name in ('normal_pass','normal_negative','normal_visual','1bpp_visual'):
            result=core.run(ROOT/'smoke'/('Mapper558_'+name+'.nes'))
            out[name]=result;print(name,result,flush=True)
    finally:core.close()
    # The negative ROM must take its red-failure path (PC differs).
    p=out['normal_pass'];n=out['normal_negative'];a=out['1bpp_visual'];b=out['normal_visual']
    normal_ok=(p['pc']!=n['pc']) and (p['pal']!=n['pal'])
    visual_ok=(a['samples']!=b['samples'] and len(set(a['samples'][:4]))>1)
    print('NORMAL BANK8K/NO ALIAS:',normal_ok,'1BPP RENDER CONTRAST:',visual_ok)
    obj={'normal_bank_ok':normal_ok,'special_video_ok':visual_ok,'runs':out}
    (ROOT/'result_mapper558_vN193.json').write_text(json.dumps(obj,indent=2)+'\n')
    if o.strict and not (normal_ok and visual_ok):raise SystemExit(1)
if __name__=='__main__':main()