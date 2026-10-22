#!/usr/bin/env python3
"""vN126 original NES2 mapper diagnostics with runtime bank signature checks.

Tests deliberately contain no copyrighted code/data. All bank pages include
identical code and RESET vectors; at offset 0x1000, byte=physical bank ID.
The program displays green ($2A) for success, red ($16) for any mismatch.
These are not complete emulation/compatibility tests.
"""
from pathlib import Path
import hashlib, json

OUT=Path(__file__).resolve().parents[1]/'smoke'
OUT.mkdir(exist_ok=True)
PR=[[3,4,2,1],[4,3,1,2],[1,2,3,4],[2,1,4,3]]

def reorder(value,source,target):
    ans=0
    for b in range(8):
        if value&(1<<b):
            ans|=1 << (target[source.index(b)] if b in source else b)
    return ans

class A:
    def __init__(self):self.code=bytearray();self.labels={};self.fixups=[]
    def mark(self,name):
        assert name not in self.labels
        self.labels[name]=len(self.code)
    def emit(self,*v):self.code+=bytes(v)
    def imm(self,op,v):self.emit(op,v&255)
    def abs(self,op,addr):self.emit(op,addr&255,(addr>>8)&255)
    def jmp(self,name):self.emit(0x4c,0,0);self.fixups.append((len(self.code)-2,name,'abs'))
    def branch(self,op,name):self.emit(op,0);self.fixups.append((len(self.code)-1,name,'rel'))
    def check(self,expected,n):
        self.abs(0xad,0xb000) # LDA $B000 mapper-dependent physical bank signature
        self.imm(0xc9,expected) # CMP #physicalBank
        self.branch(0xf0,'ok_'+str(n))
        self.jmp('fail')
        self.mark('ok_'+str(n))
    def finish(self):
        for at,name,mode in self.fixups:
            target=self.labels[name]
            if mode=='abs':self.code[at:at+2]=bytes((target&255,0x80+target//256))
            else:
                delta=target-(at+1)
                assert -128<=delta<=127,(name,delta)
                self.code[at]=delta&255
        assert len(self.code)<0x1000
        return bytes(self.code)

def chr_marker(bank):
    return ((bank & 255) ^ (((bank >> 8) & 3) * 0x5A)) & 255

def chr_test(p, expected, n):
    p.imm(0xa9,0);p.abs(0x8d,0x2006)
    p.imm(0xa9,0x80);p.abs(0x8d,0x2006)
    p.abs(0xad,0x2007) # dummy fetch, buffered
    p.abs(0xad,0x2007) # real data from $0081
    p.imm(0xc9,chr_marker(expected))
    p.branch(0xf0,'chr_ok_'+str(n))
    p.jmp('fail')
    p.mark('chr_ok_'+str(n))

def build_program(mid,sub):
    p=A()
    p.emit(0x78,0xd8) # SEI; CLD
    p.imm(0xa2,0) # LDX #0
    p.abs(0x8e,0x2000);p.abs(0x8e,0x2001) # PPUCTRL=PPUMASK=0
    if mid==4 and sub==0:
        # Control ROM for unmodified Nintendo MMC3. $5000 must not be required.
        p.imm(0xa9,7);p.abs(0x8d,0x8000)
        p.imm(0xa9,0x12);p.abs(0x8d,0x8001)
        p.check(0x12,1)
        p.imm(0xa9,0x2d);p.abs(0x8d,0x8001)
        p.check(0x2d,2)
        p.imm(0xa9,0x47);p.abs(0x8d,0x8000) # MMC3 PRG slot swap
        p.check(0x2d,3)
        p.imm(0xa9,0);p.abs(0x8d,0x8000)
        p.imm(0xa9,0x24);p.abs(0x8d,0x8001)
        chr_test(p,0x24,1)
        p.imm(0xa9,0x32);p.abs(0x8d,0x8001)
        chr_test(p,0x32,2)
    elif mid==219:
        # Write MMC3 bank #7: default outer register 1 selects 0x20 base
        p.imm(0xa9,7);p.abs(0x8d,0x8000)
        p.imm(0xa9,0x12);p.abs(0x8d,0x8001)
        p.check(0x32,1) # 0x20 | 0x12
        p.imm(0xa9,0);p.abs(0x8d,0x5001)
        p.check(0x12,2)
        p.imm(0xa9,0x40);p.abs(0x8d,0x5000) # mask 0x0F, bank 0x02
        p.check(0x02,3)
        # 2KiB MMC3 CHR register #0; read physical CHR bank from $0081.
        p.imm(0xa9,0);p.abs(0x8d,0x8000)
        p.imm(0xa9,0x24);p.abs(0x8d,0x8001)
        chr_test(p,0x24,1)
        p.imm(0xa9,0x20);p.abs(0x8d,0x5001)
        chr_test(p,0x124,2)
    else:
        # MMC3 bank 7 = $A000 slot; readable marker is from its physical 8K page.
        p.imm(0xa9,7);p.abs(0x8d,0x8000)
        p.imm(0xa9,0x12);p.abs(0x8d,0x8001)
        canonical=(mid==4 and sub==5)
        target=PR[2 if canonical else 0]
        n=1
        for mode in (0,1,3):
            p.imm(0xa9,mode);p.abs(0x8d,0x5000)
            p.check(reorder(0x12,PR[mode],target),n);n+=1
        p.imm(0xa9,0);p.abs(0x8d,0x8000)
        p.imm(0xa9,0x24);p.abs(0x8d,0x8001)
        chrpat=[[5,2,6,7,4,3],[4,5,3,2,7,6],[2,3,4,5,6,7],
                [6,4,2,3,7,5],[5,3,7,6,2,4],[4,2,5,6,7,3],
                [3,6,4,5,2,7],[2,5,6,7,3,4]]
        chrtarget=chrpat[2 if canonical else 0]
        chr_test(p,reorder(0x24,chrpat[3],chrtarget),1)
        p.imm(0xa9,1);p.abs(0x8d,0x5000)
        chr_test(p,reorder(0x24,chrpat[1],chrtarget),2)
    # Palette may be initialized while rendering is disabled; no VBlank dependency.
    p.imm(0xa2,0x2a);p.jmp('color')
    p.mark('fail')
    p.imm(0xa2,0x16)
    p.mark('color')
    p.imm(0xa9,0x3f);p.abs(0x8d,0x2006)
    p.imm(0xa9,0x00);p.abs(0x8d,0x2006)
    p.abs(0x8e,0x2007) # STX $2007, set universal BG palette 0
    p.abs(0x8e,0x2007) # STX $2007, set BG palette 1
    p.imm(0xa9,0x08);p.abs(0x8d,0x2001) # enable BG rendering
    p.mark('end');p.jmp('end')
    return p.finish()

# 8KiB bank: marker at 0x1000; vector at 0x1FFC.
# CHR 512 KiB/512 physical 1KiB pages. Signature at +$80/+81, which
# the buffered PPU $2007 read checks. All other pixel data is color 1.
chr_array=bytearray(([255]*8+[0]*8)*32768)
assert len(chr_array)==512*1024
for bank in range(512):
    marker=chr_marker(bank)
    chr_array[bank*1024+0x80]=marker
    chr_array[bank*1024+0x81]=marker
chr_data=bytes(chr_array)
manifest={}
for name,mid,sub in [('219',219,0),('249',249,0),('4_sub5',4,5),('4_standard',4,0)]:
    program=build_program(mid,sub)
    prg=bytearray()
    for bank in range(64):
        page=bytearray([0xea]*8192)
        page[:len(program)]=program
        page[0x1000]=bank
        page[0x1ffa:0x2000]=bytes([0,0x80,0,0x80,0,0x80])
        prg+=page
    h=bytearray(16)
    h[:4]=b'NES\x1a';h[4]=32;h[5]=64
    h[6]=(mid&0xf)<<4
    h[7]=(mid&0xf0)|0x08
    h[8]=((sub&0xf)<<4)|((mid>>8)&0xf)
    assert ((h[6]>>4)|(h[7]&0xf0)|((h[8]&0xf)<<8))==mid
    file=OUT/f'Aurora_Active_Diagnostic_{name}.nes'
    raw=bytes(h)+bytes(prg)+chr_data
    file.write_bytes(raw)
    manifest[file.name]={'mapper':mid,'submapper':sub,'sha256':hashlib.sha256(raw).hexdigest(),
       'prg_bytes':len(prg),'chr_bytes':len(chr_data), 'program_bytes':len(program),
       'prg_checks':3,'chr_checks':2,'success_palette_3f01':'$2A','failure_palette_3f01':'$16',
       'status':'Not tested in full QuickNES or PS2'}
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2,ensure_ascii=False)+'\n')
print('PASS: 4 active NES2 ROMs, 3 live PRG and 2 buffered PPU CHR checks each')