#!/usr/bin/env python3
"""vN184: verify that FDS expansion actually reaches libretro PCM.
Capture each ROM in an independent process, avoiding libretro global-state reuse.
"""
import ctypes as C, subprocess, sys, json, hashlib
from pathlib import Path
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parent/'experimental_vn163'))
from run_actual_quicknes_libretro import Headless, Game, BATCH

def capture(core, mode):
    h=Headless(core)
    l=h.lib
    recorded=[]
    def callback(ptr,n):
        if ptr and n:
            q=C.cast(ptr,C.POINTER(C.c_int16))
            recorded.extend(int(q[i]) for i in range(int(n)*2))
        return n
    cb=BATCH(callback)
    l.retro_set_audio_sample_batch(cb)
    rom=(HERE/'smoke'/('Mapper540_FDS_'+mode+'.nes')).read_bytes()
    raw=C.create_string_buffer(rom)
    game=Game(('diagnostic-'+mode+'.nes').encode(),C.cast(raw,C.c_void_p),len(rom),None)
    if not l.retro_load_game(C.byref(game)):
        raise RuntimeError('QuickNES refused '+mode)
    for _ in range(35):l.retro_run()
    l.retro_unload_game();h.close()
    # Ignore initial APU startup transient when comparing the waveform.
    samples=recorded[8000:]
    pcm=C.create_string_buffer(len(samples)*2)
    for i,n in enumerate(samples):C.cast(pcm,C.POINTER(C.c_int16))[i]=n
    return {'frames':35,'sample_count':len(samples),'nonzero':sum(v!=0 for v in samples),
            'max_abs':max([abs(v) for v in samples],default=0),
            'clipped':sum(v==32767 or v==-32768 for v in samples),
            'energy':sum(v*v for v in samples),
            'pcm_sha256':hashlib.sha256(pcm.raw).hexdigest()}

def main():
    if len(sys.argv)==4 and sys.argv[1]=='--capture':
        print(json.dumps(capture(Path(sys.argv[2]).resolve(),sys.argv[3])))
        return
    if len(sys.argv)!=2:
        raise SystemExit('Usage: test_fds_pcm.py /absolute/path/core.so')
    core=Path(sys.argv[1]).resolve()
    metrics={}
    for mode in ('silent','active'):
        p=subprocess.run([sys.executable,str(HERE/'test_fds_pcm.py'),'--capture',str(core),mode],capture_output=True,text=True,check=True)
        metrics[mode]=json.loads(p.stdout.strip().splitlines()[-1])
    metrics['pass']=(metrics['silent']['sample_count']==metrics['active']['sample_count'] and
      metrics['active']['sample_count']>30000 and
      metrics['active']['pcm_sha256']!=metrics['silent']['pcm_sha256'] and
      metrics['active']['energy']>metrics['silent']['energy']+10000 and
      metrics['active']['max_abs']>metrics['silent']['max_abs'] and
      metrics['active']['max_abs']<32000 and metrics['active']['clipped']==0)
    (HERE/'result_fds_pcm.json').write_text(json.dumps(metrics,indent=2)+'\n')
    print(json.dumps(metrics,indent=2))
    if not metrics['pass']:
        raise SystemExit('FAIL: FDS PCM difference not confirmed')
    print('PASS: native mapper 540 FDS expansion reaches libretro PCM')
if __name__=='__main__': main()