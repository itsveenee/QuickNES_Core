#!/usr/bin/env python3
"""NES2 4MiB mapper249 test: fixed high-bank bit must match FCEUmm's 8-bit permutation."""
from pathlib import Path
import importlib.util,hashlib,json
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('base',root/'tools/build_diagnostic_roms.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
p=m.A()
p.emit(0x78,0xd8);p.imm(0xa9,0);p.abs(0x8d,0x2000);p.abs(0x8d,0x2001)
p.abs(0xad,0xf001) # read high8(bank index) at 0xE000 physical slot
p.imm(0xc9,0);p.branch(0xf0,'ok');p.jmp('fail')
p.mark('ok');p.imm(0xa2,0x2a);p.jmp('color')
p.mark('fail');p.imm(0xa2,0x16)
p.mark('color');p.imm(0xa9,0x3f);p.abs(0x8d,0x2006);p.imm(0xa9,0);p.abs(0x8d,0x2006)
p.abs(0x8e,0x2007);p.abs(0x8e,0x2007)
p.mark('end');p.jmp('end')
program=p.finish()
prg=bytearray()
for n in range(512):
 page=bytearray([0xea]*8192)
 page[:len(program)]=program;page[0x1000]=n&255;page[0x1001]=(n>>8)&255
 page[0x1ffa:0x2000]=bytes([0,0x80,0,0x80,0,0x80]);prg+=page
h=bytearray(16);h[:4]=b'NES\x1a';h[4]=0;h[5]=64;h[6]=0x90;h[7]=0xf8;h[8]=0;h[9]=0x01
raw=bytes(h)+bytes(prg)+m.chr_data
f=root/'smoke/Aurora_HighBank_249_4MiB.nes';f.write_bytes(raw)
print(json.dumps({'name':f.name,'sha256':hashlib.sha256(raw).hexdigest(),'size':len(raw),'prg_bank_count':512,'NES2_prg_hi_nibble':1}))