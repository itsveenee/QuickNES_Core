#!/usr/bin/env python3
"""vN189: require increasing fixed FDS volume, distinct pitch and no clipping."""
import subprocess,sys,json
from pathlib import Path
HERE=Path(__file__).resolve().parent
if len(sys.argv)!=2:raise SystemExit('Usage: test_fds_matrix.py core.so')
core=Path(sys.argv[1]).resolve();out={}
for mode in ['vol08','vol16','vol31','pitch100']:
 p=subprocess.run([sys.executable,str(HERE/'test_fds_pcm.py'),'--capture',str(core),mode],capture_output=True,text=True,check=True)
 out[mode]=json.loads(p.stdout.strip().splitlines()[-1])
energies=[out[m]['energy'] for m in ['vol08','vol16','vol31']]
checks={
 'monotonic_volume':0<energies[0]<energies[1]<energies[2],
 'different_pitch':out['vol31']['pcm_sha256']!=out['pitch100']['pcm_sha256'],
 'no_clipping':all(x['clipped']==0 and x['max_abs']<32000 for x in out.values()),
 'has_audio':all(x['nonzero']>10000 for x in out.values()),
 'matching_frame_count':len({x['sample_count'] for x in out.values()})==1
}
out['checks']=checks;out['pass']=all(checks.values())
(HERE/'result_fds_matrix.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps(out,indent=2));
if not out['pass']:raise SystemExit('FAIL: FDS multi-volume/pitch matrix')
print('PASS: native 540 FDS multi-volume and pitch PCM')