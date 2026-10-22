#!/usr/bin/env python3
"""Original NES 2.0 MMC5 ExRAM mode / nametable / fill tests.
Generated code is original and does not include a commercial ROM.
"""
from pathlib import Path
import json,hashlib
OUT=Path(__file__).resolve().parent/'smoke';OUT.mkdir(parents=True,exist_ok=True)
class Code:
 def __init__(self):self.origin=0xe000;self.b=bytearray();self.lab={};self.fix=[];self.n=0
 def emit(self,*a):self.b.extend(bytes(a))
 def mark(self,n):self.lab[n]=self.origin+len(self.b)
 def imm(self,op,n):self.emit(op,n&255)
 def ab(self,op,n):self.emit(op,n&255,n>>8&255)
 def sta(self,v,addr):self.imm(0xa9,v);self.ab(0x8d,addr)
 def jmp(self,n):self.emit(0x4c,0,0);self.fix.append((len(self.b)-2,n,False))
 def branch(self,op,n):self.emit(op,0);self.fix.append((len(self.b)-1,n,True))
 def check(self,addr,value):
  n='good_'+str(self.n);self.n+=1;self.ab(0xad,addr);self.imm(0xc9,value);self.branch(0xf0,n);self.jmp('fail');self.mark(n)
 def check_not(self,addr,value):
  n='good_'+str(self.n);self.n+=1;self.ab(0xad,addr);self.imm(0xc9,value);self.branch(0xd0,n);self.jmp('fail');self.mark(n)
 def check_vram(self,addr,value):
  self.sta(addr>>8,0x2006);self.sta(addr&255,0x2006)
  self.ab(0xad,0x2007);self.ab(0xad,0x2007)
  n='good_'+str(self.n);self.n+=1;self.imm(0xc9,value);self.branch(0xf0,n);self.jmp('fail');self.mark(n)
 def init(self):self.emit(0x78,0xd8);self.sta(0x40,0x4017);self.sta(0,0x4010);self.sta(0,0x2000);self.sta(0,0x2001)
 def finish(self):
  self.imm(0xa2,0x2a);self.jmp('color');self.mark('fail');self.imm(0xa2,0x16)
  self.mark('color');self.sta(0x3f,0x2006);self.sta(0,0x2006);self.ab(0x8e,0x2007);self.ab(0x8e,0x2007);self.sta(8,0x2001);self.mark('end');self.jmp('end')
 def code(self):
  for pos,key,rel in self.fix:
   dest=self.lab[key]
   if rel:
    delta=dest-(self.origin+pos+1);assert -128<=delta<=127,(key,delta);self.b[pos]=delta&255
   else:self.b[pos:pos+2]=dest.to_bytes(2,'little')
  return self.b

def rom(kind,bad=False):
 p=Code();p.init()
 if kind=='cpu':
  # On reset $5104=%11, ExRAM must be read-only.
  p.sta(0x57,0x5c24);p.check(0x5c24,0)
  p.sta(2,0x5104);p.sta(0x5a,0x5c24);p.check(0x5c24,0x5a)
  p.sta(3,0x5104);p.sta(0xb2,0x5c24);p.check(0x5c24,0x5a)
  p.sta(0,0x5104);p.check_not(0x5c24,0x5a) # write-only reads open bus
  p.sta(0x77,0x5c24) # during blanking hardware stores zero
  p.sta(2,0x5104);p.check(0x5c24,0x77 if bad else 0)
  p.sta(1,0x5104);p.sta(0x33,0x5c24)
  p.sta(2,0x5104);p.check(0x5c24,0)
  p.sta(0x91,0x5fff);p.check(0x5fff,0x91)
 elif kind=='nt':
  p.sta(2,0x5104);p.sta(0x7b,0x5c20);p.sta(0x36,0x5c80)
  p.sta(0,0x5104);p.sta(2,0x5105)  # NT0 from ExRAM; other three CIRAM0
  p.check_vram(0x2020,0x00 if bad else 0x7b);p.check_vram(0x2080,0x36)
  # In CPU memory mode ExRAM must NOT be a usable PPU nametable.
  p.sta(2,0x5104);p.check_vram(0x2020,0)
  p.sta(0,0x5104);p.check_vram(0x2020,0x7b)
 elif kind=='quadrants':
  p.sta(2,0x5104);p.sta(0x62,0x5c10);p.sta(0xa5,0x5fc0)
  p.sta(0,0x5104);p.sta(0xaa,0x5105) # all four NT quadrants = ExRAM
  for address in (0x2010,0x2410,0x2810,0x2c10):
   p.check_vram(address,0 if bad and address==0x2010 else 0x62)
  p.check_vram(0x23c0,0xa5);p.check_vram(0x2fc0,0xa5)
  p.sta(0xe4,0x5105) # CIRAM0, CIRAM1, ExRAM, fill
  p.sta(0x3e,0x5106);p.sta(2,0x5107)
  p.check_vram(0x2810,0x62);p.check_vram(0x2c10,0x3e);p.check_vram(0x2fc0,0xaa)
 elif kind=='render':
  # Rendering is enabled, and the CPU waits ~8k cycles into scanline 0..239.
  # In MMC5 modes 0/1, CPU writes during rendering MUST store data.
  p.sta(0,0x5104);p.sta(8,0x2001)
  p.imm(0xa2,24);p.mark('outer');p.imm(0xa0,64);p.mark('inner')
  p.emit(0x88);p.branch(0xd0,'inner');p.emit(0xca);p.branch(0xd0,'outer')
  p.sta(0x6d,0x5c31);p.sta(2,0x5104)
  p.check(0x5c31,0x00 if bad else 0x6d)
  p.sta(0,0x2001)
 elif kind=='fill':
  p.sta(0x03,0x5105);p.sta(0x39,0x5106);p.sta(2,0x5107)
  p.check_vram(0x2000,0x38 if bad else 0x39)
  p.check_vram(0x23c0,0xaa)
  # Fill nametable is read-only via PPUDATA.
  p.sta(0x20,0x2006);p.sta(0,0x2006);p.sta(0x78,0x2007)
  p.check_vram(0x2000,0x39)
 else: raise Exception(kind)
 p.finish();code=p.code();prg=bytearray(4*16*1024)
 off=len(prg)-8192;prg[off:off+len(code)]=code
 for vec in (0x1ffa,0x1ffc,0x1ffe):prg[off+vec:off+vec+2]=(0xe000).to_bytes(2,'little')
 header=bytearray(16);header[:4]=b'NES\x1a';header[4]=len(prg)//16384;header[5]=1;header[6]=0x50;header[7]=8
 return bytes(header)+bytes(prg)+bytes(8192)

manifest={}
for kind in ('cpu','nt','fill','render','quadrants'):
 for bad in (False,True):
  fname='MMC5_ExRAM_'+kind+('_negative' if bad else '_positive')+'.nes';b=rom(kind,bad);(OUT/fname).write_bytes(b)
  manifest[fname]={'expect':'red' if bad else 'green','mapper':5,'sha256':hashlib.sha256(b).hexdigest(),'size':len(b)}
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Generated',len(manifest),'original MMC5 ROMs in',OUT)