#!/usr/bin/env python3
"""Generate original iNES/NES2 mapper 35/209 banking and ALU diagnostics.

No third-party ROM or assets are copied.  Test coverage is restricted to
PRG/CHR bank selection and multiplication/adder registers; physical fetch
IRQ clocks, all CPU bus timings and CHR-RAM protection are NOT covered.
"""
from pathlib import Path
import hashlib, json

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'smoke'
OUT.mkdir(parents=True, exist_ok=True)

class Asm:
    def __init__(self): self.mem=bytearray();self.labels={};self.fixups=[]
    def put(self,*vals): self.mem.extend(bytes(vals))
    def mark(self,n):
        assert n not in self.labels,n
        self.labels[n]=len(self.mem)
    def imm(self,op,v):self.put(op,v&255)
    def absolute(self,op,addr):self.put(op,addr&255,(addr>>8)&255)
    def jmp(self,label):self.put(0x4c,0,0);self.fixups.append((len(self.mem)-2,label,'abs'))
    def branch(self,op,label):self.put(op,0);self.fixups.append((len(self.mem)-1,label,'rel'))
    def sta_imm(self,value,addr):self.imm(0xa9,value);self.absolute(0x8d,addr)
    def compare(self,addr,value,tag):
        self.absolute(0xad,addr);self.imm(0xc9,value)
        self.branch(0xf0,'ok_'+tag);self.jmp('fail');self.mark('ok_'+tag)
    def compare_chr(self,bank,tag):
        self.sta_imm(0,0x2006);self.sta_imm(0x80,0x2006)
        self.absolute(0xad,0x2007);self.absolute(0xad,0x2007)
        marker=(bank&255)^((((bank>>8)&3)*0x5a)&255)
        self.imm(0xc9,marker);self.branch(0xf0,'ok_'+tag);self.jmp('fail');self.mark('ok_'+tag)
    def finalize(self):
        for index,name,kind in self.fixups:
            dest=self.labels[name]
            if kind=='abs':self.mem[index:index+2]=bytes((dest&255,0x80+dest//256))
            else:
                delta=dest-index-1
                assert -128<=delta<=127,(name,delta)
                self.mem[index]=delta&255
        assert len(self.mem)<4096, len(self.mem)
        return bytes(self.mem)

def prog():
    p=Asm();p.put(0x78,0xd8) # sei,cld
    p.sta_imm(0x40,0x4017);p.sta_imm(0,0x4010) # disable APU IRQ
    p.sta_imm(0,0x2000);p.sta_imm(0,0x2001)
    p.sta_imm(2,0xd000) # 8KiB bank mode
    p.sta_imm(0x12,0x8001);p.compare(0xb000,0x12,'prg1')
    p.sta_imm(0x2d,0x8001);p.compare(0xb000,0x2d,'prg2')
    p.sta_imm(3,0xd000) # reverse 7-bit PRG banks
    p.sta_imm(0x12,0x8001);p.compare(0xb000,0x24,'prg3')
    # ALU multiply, incrementing adder and clearing adder on write to test
    p.sta_imm(11,0x5800);p.sta_imm(13,0x5801)
    p.compare(0x5800,143,'alu_low');p.compare(0x5801,0,'alu_high')
    p.sta_imm(10,0x5802);p.sta_imm(7,0x5802);p.compare(0x5802,17,'adder')
    p.sta_imm(44,0x5803);p.compare(0x5802,0,'adder_clear');p.compare(0x5803,44,'test_reg')
    # CHR 8KiB -> 2KiB -> 1KiB bank mapping
    p.sta_imm(0,0xd000)
    p.sta_imm(3,0x9000);p.compare_chr(24,'chr1')
    p.sta_imm(0x10,0xd000);p.compare_chr(6,'chr2')
    p.sta_imm(0x18,0xd000);p.compare_chr(3,'chr3')
    p.imm(0xa2,0x2a);p.jmp('color');p.mark('fail');p.imm(0xa2,0x16)
    p.mark('color');p.sta_imm(0x3f,0x2006);p.sta_imm(0,0x2006)
    p.absolute(0x8e,0x2007);p.absolute(0x8e,0x2007)
    p.sta_imm(0x08,0x2001)
    p.mark('end');p.jmp('end')
    return p.finalize()

P=prog()
assert P
chr_array=bytearray(([255]*8+[0]*8)*32768)
assert len(chr_array)==512*1024
for b in range(512):
    m=(b&255)^((((b>>8)&3)*0x5a)&255)
    chr_array[b*1024+0x80]=m;chr_array[b*1024+0x81]=m
CHRS=bytes(chr_array)
records={}
for mapper in (35,209):
    prg=bytearray()
    for bank in range(64):
        page=bytearray([0xea]*8192)
        page[:len(P)]=P;page[0x1000]=bank
        page[0x1ffa:0x2000]=bytes([0,0x80,0,0x80,0,0x80])
        prg.extend(page)
    h=bytearray(16);h[:4]=b'NES\x1a';h[4]=32;h[5]=64
    h[6]=((mapper&15)<<4)
    h[7]=(mapper&0xf0)|0x08
    h[8]=(mapper>>8)&15
    path=OUT/f'Aurora_JYASIC_{mapper}_PRG_CHR_ALU.nes'
    blob=bytes(h)+bytes(prg)+CHRS
    path.write_bytes(blob)
    records[path.name]={'sha256':hashlib.sha256(blob).hexdigest(),'mapper':mapper,
      'size_bytes':len(blob),'prg_checks':3,'chr_checks':3,'alu_checks':5,
      'expected_ppu_palette':[0x2a,0x2a], 'not_tested':['PPU physical fetch IRQ','IRQ all-CPU write correctness','CHR-RAM write protection','commercial ROMs','PS2 hardware']}
(OUT/'jyasic_manifest.json').write_text(json.dumps(records,indent=2)+'\n')
print('Created',len(records),'original JYASIC PRG/CHR/ALU ROMs, program bytes',len(P))