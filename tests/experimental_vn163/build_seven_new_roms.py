#!/usr/bin/env python3
"""vN163 - Original diagnostics for mapper 272, 540, 558, no commercial ROM.
These probe basic PRG, CHR and CPU-visible registers ONLY, not physical PPU
fetch timing, complete FDS audio, or EEPROM persistence.
"""
from pathlib import Path
import json,hashlib
OUT=Path(__file__).parent/'smoke'; OUT.mkdir(exist_ok=True)
class A:
 def __init__(self,origin):self.origin=origin;self.buf=bytearray();self.lab={};self.fix=[];self.n=0
 def emit(self,*b):self.buf.extend(bytes(b))
 def mark(self,n):self.lab[n]=self.origin+len(self.buf)
 def imm(self,opc,n):self.emit(opc,n&255)
 def ab(self,opc,n):self.emit(opc,n&255,(n>>8)&255)
 def sta(self,v,a):self.imm(0xa9,v);self.ab(0x8d,a)
 def check(self,a,v):
  n='ok'+str(self.n);self.n+=1;self.ab(0xad,a);self.imm(0xc9,v);self.br(0xf0,n);self.jmp('fail');self.mark(n)
 def br(self,op,l):self.emit(op,0);self.fix.append((len(self.buf)-1,l,True))
 def jmp(self,l):self.emit(0x4c,0,0);self.fix.append((len(self.buf)-2,l,False))
 def check_chr(self,bank,kind='1k'):
  self.sta(0,0x2006);self.sta(0x80,0x2006)
  self.ab(0xad,0x2007);self.ab(0xad,0x2007)
  self.imm(0xc9,bank&255)
  n='chr'+str(self.n);self.n+=1;self.br(0xf0,n);self.jmp('fail');self.mark(n)
 def finish(self):
  self.imm(0xa2,0x2a);self.jmp('color');self.mark('fail');self.imm(0xa2,0x16)
  self.mark('color');self.sta(0x3f,0x2006);self.sta(0,0x2006);self.ab(0x8e,0x2007);self.ab(0x8e,0x2007);self.sta(8,0x2001);self.mark('end');self.jmp('end')
 def code(self):
  for i,l,rel in self.fix:
   adr=self.lab[l];
   if rel:
    off=adr-(self.origin+i+1);assert -128<=off<=127,(l,off);self.buf[i]=off&255
   else:self.buf[i:i+2]=adr.to_bytes(2,'little')
  return bytes(self.buf)
def header(mapper,prg_size,chr_size):
 assert prg_size%(16*1024)==0 and chr_size%(8*1024)==0, (mapper,prg_size,chr_size)
 b=bytearray(16);b[:4]=b'NES\x1a';b[4]=prg_size//(16*1024);b[5]=chr_size//(8*1024)
 assert b[4]<256 and b[5]<256
 b[6]=(mapper&15)<<4;b[7]=(mapper&0xf0)|0x08;b[8]=(mapper>>8)&15
 return bytes(b)
def initialize(p):p.emit(0x78,0xd8);p.sta(0x40,0x4017);p.sta(0,0x4010);p.sta(0,0x2000);p.sta(0,0x2001)
def rom272(bad=False):
 p=A(0xe000);initialize(p)
 p.sta(5,0x8000);p.check(0x8002,255 if bad else 5)
 p.sta(6,0xa000);p.check(0xa002,6)
 p.check(0xc002,14)
 p.sta(10,0xb000);p.sta(0,0xb001);p.check_chr(10)
 p.finish();prog=p.code();prg=bytearray(16*8192)
 for b in range(16):prg[b*8192+2]=b
 prg[-8192:-8192+len(prog)]=prog
 for vec in (0x1ffa,0x1ffc,0x1ffe):prg[len(prg)-8192+vec:len(prg)-8192+vec+2]=(0xe000).to_bytes(2,'little')
 chr=bytearray(32*1024)
 for b in range(32):chr[b*1024+0x80]=b;chr[b*1024+0x81]=b
 return header(272,len(prg),len(chr))+prg+chr
def rom540(bad=False):
 p=A(0xe000);initialize(p)
 p.sta(7,0x8000);p.check(0x8002,255 if bad else 7)
 p.sta(9,0x8001);p.check(0xa002,9)
 p.sta(11,0x8002);p.check(0xc002,11)
 p.sta(5,0xa000);p.check_chr(5,'2k')
 p.finish();prog=p.code();prg=bytearray(64*8192)
 for b in range(64):prg[b*8192+2]=b
 prg[-8192:-8192+len(prog)]=prog
 for vec in (0x1ffa,0x1ffc,0x1ffe):prg[len(prg)-8192+vec:len(prg)-8192+vec+2]=(0xe000).to_bytes(2,'little')
 chr=bytearray(16*2048)
 for b in range(16):chr[b*2048+0x80]=b;chr[b*2048+0x81]=b
 return header(540,len(prg),len(chr))+prg+chr
def rom558(bad=False):
 p=A(0x8000);initialize(p)
 p.check(0x9000,3) # initial mapping bank 3
 p.sta(4,0x5200);p.check(0x5000,0 if bad else 4)
 p.sta(0,0x5200);p.check(0x5000,0)
 p.sta(6,0x5300);p.check(0x9000,0) # bank 0 after switching $5300 bit2
 p.sta(4,0x5200);p.check(0x5000,4)
 p.finish();prog=p.code();prg=bytearray(16*32768)
 for b in range(16):
  off=b*32768;prg[off:off+len(prog)]=prog
  prg[off+0x1000]=b
  for vec in (0x7ffa,0x7ffc,0x7ffe):prg[off+vec:off+vec+2]=(0x8000).to_bytes(2,'little')
 chr=bytes(8*1024)
 return header(558,len(prg),len(chr))+prg+chr
manifest={}
for m,fn in ((272,rom272),(540,rom540),(558,rom558)):
 for corrupt in (False,True):
  name=f'Aurora_vN163_mapper{m}_'+('negative' if corrupt else 'positive')+'.nes'
  data=fn(corrupt);(OUT/name).write_bytes(data)
  manifest[name]={'sha256':hashlib.sha256(data).hexdigest(),'size':len(data),'expect':'red' if corrupt else 'green','mapper':m}
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Generated',len(manifest),'original ROMs',sum(x['size'] for x in manifest.values()),'bytes')