#!/usr/bin/env python3
"""Original deterministic CHR-RAM write-protection tests for JYASIC 35/209.
Firmware-independent 6502 instructions; negative controls must display error.
"""
from pathlib import Path
import json,hashlib

HERE=Path(__file__).resolve().parent
OUT=HERE.parent/'smoke'
OUT.mkdir(parents=True,exist_ok=True)
class Asm:
    def __init__(self):self.mem=bytearray();self.labels={};self.fixups=[]
    def put(self,*v):self.mem.extend(bytes(v))
    def label(self,name):self.labels[name]=len(self.mem)
    def imm(self,op,data):self.put(op,data&255)
    def abs(self,op,addr):self.put(op,addr&255,(addr>>8)&255)
    def sta(self,v,addr):self.imm(0xa9,v);self.abs(0x8d,addr)
    def jump(self,name):self.put(0x4c,0,0);self.fixups.append((len(self.mem)-2,name,'abs'))
    def branch(self,opcode,name):self.put(opcode,0);self.fixups.append((len(self.mem)-1,name,'rel'))
    def vram_set(self,addr):self.sta((addr>>8)&255,0x2006);self.sta(addr&255,0x2006)
    def vram_read_compare(self,value,tag):
        self.vram_set(0x0000)
        self.abs(0xad,0x2007);self.abs(0xad,0x2007)
        self.imm(0xc9,value);self.branch(0xf0,'ok'+tag);self.jump('fail');self.label('ok'+tag)
    def finish(self):
        for off,name,typ in self.fixups:
            dest=self.labels[name]
            if typ=='abs':self.mem[off:off+2]=bytes([dest&255,0x80+(dest//256)])
            else:
                d=dest-off-1
                if not -128<=d<=127:raise ValueError((name,d))
                self.mem[off]=d&255
        return bytes(self.mem)

def build(negative=False):
    p=Asm();p.put(0x78,0xd8)
    p.sta(0x40,0x4017);p.sta(0,0x4010);p.sta(0,0x2000);p.sta(0,0x2001)
    p.sta(0,0xd000);p.sta(0,0xd002)
    p.vram_set(0);p.sta(0x5a,0x2007)
    p.vram_read_compare(0x5a,'before')
    # On the negative ROM, never enable hardware write protection, but
    # still require the previous data. The ROM must take the error branch.
    p.sta(0 if negative else 0x40,0xd002)
    p.vram_set(0);p.sta(0xa5,0x2007)
    p.vram_read_compare(0x5a,'protected')
    p.sta(0,0xd002)
    p.vram_set(0);p.sta(0x33,0x2007)
    p.vram_read_compare(0x33,'unprotected')
    p.imm(0xa2,0x2a);p.jump('color');p.label('fail');p.imm(0xa2,0x16)
    p.label('color');p.sta(0x3f,0x2006);p.sta(0,0x2006)
    p.abs(0x8e,0x2007);p.abs(0x8e,0x2007)
    p.sta(0x08,0x2001);p.label('end');p.jump('end')
    return p.finish()

manifest={}
for mapper in (35,209):
    for neg in (False,True):
        code=build(neg)
        prg=bytearray()
        for _ in range(64):
            bank=bytearray([0xea]*8192)
            bank[:len(code)]=code
            bank[0x1ffa:0x2000]=bytes([0,0x80,0,0x80,0,0x80])
            prg.extend(bank)
        head=bytearray(16);head[:4]=b'NES\x1a';head[4]=32;head[5]=0
        head[6]=((mapper&15)<<4);head[7]=(mapper&0xf0)|0x08;head[8]=(mapper>>8)&15
        name=f'Aurora_JYPROTECT_{mapper}_{"negative" if neg else "protected"}.nes'
        blob=bytes(head)+bytes(prg)
        (OUT/name).write_bytes(blob)
        manifest[name]={'mapper':mapper,'negative':neg,'expected_palette':'16' if neg else '2a','sha256':hashlib.sha256(blob).hexdigest(),'rom_bytes':len(blob)}
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Generated 4 real CPU/PPU bus ROMs, each with 3 $2007 readbacks')