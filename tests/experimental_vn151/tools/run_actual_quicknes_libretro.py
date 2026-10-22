#!/usr/bin/env python3
"""Execute the vN126 active NES2 ROMs in the ACTUAL QuickNES libretro .so.

This is stronger than the restricted mapper/CPU harness. It uses libretro API
via ctypes. The Linux build is not PS2 runtime validation. The callbacks and
pixel format intentionally support the RGB565 QuickNES software renderer.
"""
import ctypes as C
import argparse
from pathlib import Path
import sys,os

class Game(C.Structure):
    _fields_=[('path',C.c_char_p),('data',C.c_void_p),('size',C.c_size_t),('meta',C.c_char_p)]

ENV=C.CFUNCTYPE(C.c_bool,C.c_uint,C.c_void_p)
VIDEO=C.CFUNCTYPE(None,C.c_void_p,C.c_uint,C.c_uint,C.c_size_t)
AUDIO=C.CFUNCTYPE(None,C.c_int16,C.c_int16)
BATCH=C.CFUNCTYPE(C.c_size_t,C.c_void_p,C.c_size_t)
POLL=C.CFUNCTYPE(None)
STATE=C.CFUNCTYPE(C.c_int16,C.c_uint,C.c_uint,C.c_uint,C.c_uint)

class Headless:
    def __init__(self, core):
        self.lib=C.CDLL(str(core))
        self.lib.retro_set_environment.argtypes=[ENV]
        self.lib.retro_set_video_refresh.argtypes=[VIDEO]
        self.lib.retro_set_audio_sample.argtypes=[AUDIO]
        self.lib.retro_set_audio_sample_batch.argtypes=[BATCH]
        self.lib.retro_set_input_poll.argtypes=[POLL]
        self.lib.retro_set_input_state.argtypes=[STATE]
        self.lib.retro_load_game.argtypes=[C.POINTER(Game)]
        self.lib.retro_load_game.restype=C.c_bool
        self.frames=[]
        def env(cmd, ptr):
            # The QuickNES software renderer specifically requests RGB565.
            # The normal libretro.h constant for SET_PIXEL_FORMAT is 10.
            if cmd==10 and ptr:
                fmt=C.cast(ptr,C.POINTER(C.c_int)).contents.value
                return fmt==2 # RETRO_PIXEL_FORMAT_RGB565
            return False
        def video(ptr, width, height, pitch):
            if not ptr or not width or not height:return
            x=width//2;y=height//2
            raw=C.string_at(int(ptr)+y*pitch+x*2,2)
            pixel=int.from_bytes(raw,'little')
            r=(pixel>>11)&31;g=(pixel>>5)&63;b=pixel&31
            self.frames.append((r,g,b,width,height))
        def audio(left,right): pass
        def batch(ptr,n):return n
        def poll(): pass
        def state(a,b,c,d):return 0
        self.callbacks=(ENV(env),VIDEO(video),AUDIO(audio),BATCH(batch),POLL(poll),STATE(state))
        self.lib.retro_set_environment(self.callbacks[0])
        self.lib.retro_set_video_refresh(self.callbacks[1])
        self.lib.retro_set_audio_sample(self.callbacks[2])
        self.lib.retro_set_audio_sample_batch(self.callbacks[3])
        self.lib.retro_set_input_poll(self.callbacks[4])
        self.lib.retro_set_input_state(self.callbacks[5])
        self.lib.retro_init()
    def load_and_run(self, rom, frames=30):
        self.frames.clear()
        data=rom.read_bytes()
        buf=C.create_string_buffer(data)
        g=Game(str(rom).encode(),C.cast(buf,C.c_void_p),len(data),None)
        ok=self.lib.retro_load_game(C.byref(g))
        if not ok:raise RuntimeError('retro_load_game rejected '+rom.name)
        try:
            for _ in range(frames):self.lib.retro_run()
            if not self.frames:raise RuntimeError('Core produced no video frames')
            self.lib.retro_serialize_size.restype=C.c_size_t
            size=self.lib.retro_serialize_size()
            if size<=0:raise RuntimeError('QuickNES did not expose a savestate')
            self.lib.retro_serialize.argtypes=[C.c_void_p,C.c_size_t]
            self.lib.retro_serialize.restype=C.c_bool
            self.lib.retro_unserialize.argtypes=[C.c_void_p,C.c_size_t]
            self.lib.retro_unserialize.restype=C.c_bool
            def snapshot():
                b=C.create_string_buffer(size)
                if not self.lib.retro_serialize(b,size):
                    raise RuntimeError('QuickNES state serialization failed')
                return b.raw
            def block(state,tag):
                index=state.find(tag)
                if index<0:raise RuntimeError('Missing state block '+repr(tag))
                length=int.from_bytes(state[index+4:index+8],'little')
                if index+8+length>len(state):raise RuntimeError('Out-of-bounds block '+repr(tag))
                return state[index+8:index+8+length]
            state=snapshot()
            ppu=block(state,b'PPUR');cpu=block(state,b'CPUR');mapr=block(state,b'MAPR')
            self.last_mapr=mapr
            if len(ppu)<14 or len(cpu)<2:raise RuntimeError('Short CPU/PPU state')
            bg0,bg1=ppu[12],ppu[13]
            pc=int.from_bytes(cpu[:2],'little')
            # A real save/load roundtrip, not just serialization. The native
            # mapper-specific state must restore exactly after another frame.
            for _ in range(2):self.lib.retro_run()
            src=C.create_string_buffer(state)
            if not self.lib.retro_unserialize(src,size):
                raise RuntimeError('QuickNES state restore failed')
            restored=snapshot()
            if block(restored,b'MAPR')!=mapr:
                raise RuntimeError('Mapper state changed during roundtrip')
            ppu2=block(restored,b'PPUR')
            cpu2=block(restored,b'CPUR')
            if ppu2[12:14]!=ppu[12:14] or cpu2[:2]!=cpu[:2]:
                raise RuntimeError('CPU/PPU state roundtrip mismatch')
            for _ in range(2):self.lib.retro_run()
            ppu3=block(snapshot(),b'PPUR')
            if ppu3[12:14]!=ppu[12:14]:
                raise RuntimeError('Palette lost after restored core resumed')
            if os.environ.get('AURORA_DUMP_STATE'):
                print('STATE:',len(state),'MAPR=',len(mapr),'PPU palette:',hex(bg0),hex(bg1),'PC=',hex(pc),flush=True)
            return list(self.frames),bg0,bg1,pc,len(mapr)
        finally:self.lib.retro_unload_game()
    def close(self):self.lib.retro_deinit()

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('core',type=Path)
    ap.add_argument('roms',nargs='+',type=Path)
    ap.add_argument('--frames',type=int,default=30)
    ap.add_argument('--expect-color',choices=['green','red'],default='green')
    args=ap.parse_args()
    if not args.core.exists():ap.error('Missing native .so: '+str(args.core))
    machine=Headless(args.core)
    try:
        for rom in args.roms:
            data,bg0,bg1,pc,mapr_size=machine.load_and_run(rom,args.frames)
            print('ROM',rom.name,'last eight frames RGB565',data[-8:],flush=True)
            # A sampled RGB565 pixel is not a reliable diagnostic (it can
            # come from a clipped/offscreen/overscan region). Read the actual
            # PPUR palette registers from the QuickNES savestate instead.
            expected=0x2a if args.expect_color=='green' else 0x16
            print('   PPU palette $3F00/$3F01:',hex(bg0),hex(bg1),'CPU PC:',hex(pc),flush=True)
            if not (bg0==expected and bg1==expected):
                raise RuntimeError('QuickNES palette does not match %s: %s got $%02X/$%02X'%
                    (args.expect_color,rom.name,bg0,bg1))
            expected_mapr=50 if any(tag in rom.name for tag in ('JYASIC_35','JYASIC_209','JYIRQ_35','JYIRQ_209','JYNT_35','JYNT_209')) else 18 if '_219' in rom.name else 15 if '4_standard' in rom.name else 16
            if mapr_size!=expected_mapr:
                raise RuntimeError('Unexpected mapper state bytes: %s: %d instead of %d'%
                    (rom.name,mapr_size,expected_mapr))
            if pc<0x8020 or pc>0x8200:
                raise RuntimeError('Unexpected CPU PC, program may not have finished: '+hex(pc))
    finally:machine.close()
    print('PASS: actual QuickNES CPU/PPU + MAPR savestate roundtrip confirm',args.expect_color,'for every diagnostic ROM')
if __name__=='__main__':main()