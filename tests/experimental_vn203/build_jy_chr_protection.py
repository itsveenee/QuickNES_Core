#!/usr/bin/env python3
"""vN203: Original mapper35/209 CHR-RAM write protect ROMs; no commercial ROM content.
Hardware reference: FCEUmm JYASIC_SyncCHR, mode[2] bit 6, written through $D002.
"""
from pathlib import Path
import hashlib,json,sys
ROOT=Path(__file__).resolve().parent
OUT=ROOT/'smoke';OUT.mkdir(exist_ok=True)
class A:
 def __init__(self,origin=0x8000):self.origin=origin;self.code_=bytearray();self.labels={};self.fixups=[];self.n=0
 def emit(self,*x):self.code_.extend(x)
 def imm(self,op,val):self.emit(op,val&255)
 def abs(self,op,addr):self.emit(op,addr&255,(addr>>8)&255)
 def sta(self,v,addr):self.imm(0xa9,v);self.abs(0x8d,addr)
 def mark(self,s):assert s not in self.labels;self.labels[s]=self.origin+len(self.code_)
 def jmp(self,s):self.emit(0x4c,0,0);self.fixups.append((len(self.code_)-2,s,False))
 def beq(self,s):self.emit(0xf0,0);self.fixups.append((len(self.code_)-1,s,True))
 def addr(self,a):self.sta((a>>8)&255,0x2006);self.sta(a&255,0x2006)
 def write_chr(self,addr,val):self.addr(addr);self.sta(val,0x2007)
 def check_chr(self,addr,expected):
  self.addr(addr);self.abs(0xad,0x2007);self.abs(0xad,0x2007)
  self.imm(0xc9,expected);tag=f'ok{self.n}';self.n+=1
  self.beq(tag);self.jmp('bad');self.mark(tag)
 def finish(self):
  self.mark('good');self.imm(0xa2,0x2a);self.jmp('color');self.mark('bad');self.imm(0xa2,0x16)
  self.mark('color');self.sta(0x3f,0x2006);self.sta(0x00,0x2006)
  self.abs(0x8e,0x2007);self.abs(0x8e,0x2007);self.sta(8,0x2001);self.mark('end');self.jmp('end')
 def code(self):
  for pos,s,isrel in self.fixups:
   tgt=self.labels[s]
   if isrel:
    delta=tgt-(self.origin+pos+1);assert -128<=delta<=127,(s,delta);self.code_[pos]=delta&255
   else:self.code_[pos:pos+2]=tgt.to_bytes(2,'little')
  assert len(self.code_)<0x1000,len(self.code_)
  return bytes(self.code_)
def build(mapper,negative,multi=False):
 p=A();p.emit(0x78,0xd8);p.sta(0x40,0x4017);p.sta(0,0x4010);p.sta(0,0x2000);p.sta(0,0x2001)
 # Write 4 different CHR-RAM pages in multi-page mode, including $1F80.
 # All writes must be blocked with mode[2].6=1; no tile dirtiness may leak.
 pages=[0x0080,0x0480,0x1080,0x1f80] if multi else [0x0080]
 p.sta(0x00,0xd000)  # 8 KiB CHR banking, distinct physical 1 KiB pages
 p.sta(0x00,0xd002)
 for i,addr in enumerate(pages):
  val=0x33+i
  p.write_chr(addr,val);p.check_chr(addr,val)
 p.sta(0x40,0xd002)
 for i,addr in enumerate(pages):
  p.write_chr(addr,0x77+i)
  p.check_chr(addr,(0x77+i) if (negative and i==0) else (0x33+i))
 p.sta(0x00,0xd002)
 for i,addr in enumerate(pages):
  p.write_chr(addr,0x55+i);p.check_chr(addr,0x55+i)
 p.finish();code=p.code()
 prg=bytearray([0xea])*(64*8192)
 for b in range(64):
  off=b*8192;prg[off:off+len(code)]=code
  for vec in (0x1ffa,0x1ffc,0x1ffe):prg[off+vec:off+vec+2]=(0x8000).to_bytes(2,'little')
 head=bytearray(16);head[:4]=b'NES\x1a';head[4]=32;head[5]=0;head[6]=(mapper&15)<<4;head[7]=(mapper&0xf0)|8;head[8]=(mapper>>8)&15
 data=bytes(head)+bytes(prg)
 return data,len(code)
entries={}
for mapper in (35,209):
 for multi in (False,True):
  for negative in (False,True):
   name=f'Aurora_JYCHR_{mapper}_'+('multipage_' if multi else '')+('negative' if negative else 'positive')+'.nes'
   data,size=build(mapper,negative,multi);(OUT/name).write_bytes(data)
   entries[name]={'mapper':mapper,'negative':negative,'multipage':multi,'expected':'red' if negative else 'green','prg_bytes':len(data)-16,'code_bytes':size,'sha256':hashlib.sha256(data).hexdigest()}
(OUT/'manifest_chr_protection.json').write_text(json.dumps(entries,indent=2)+'\n')
print('Generated',len(entries),'mapper 35/209 CHR-RAM protection diagnostic ROMs')