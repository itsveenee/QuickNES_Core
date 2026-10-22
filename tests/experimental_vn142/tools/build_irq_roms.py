#!/usr/bin/env python3
"""Build redistributable NES2 MMC3 IRQ probes (original source, not game dumps)."""
from pathlib import Path
import hashlib,json
out=Path(__file__).resolve().parents[1]/'smoke';out.mkdir(parents=True,exist_ok=True)
# Machine program run from identical PRG banks at $8000; IRQ entry at $8100.
# $0010 = 0x2a on IRQ, or 0x00 otherwise. $0011 counts IRQ entries.
def make_program(ctrl,mask,latch):
    b=bytearray([0xea]*0x200)
    i=0
    def emit(*args):
        nonlocal i
        b[i:i+len(args)]=bytes(args);i+=len(args)
    def imm(code,v): emit(code,v)
    def sta(addr): emit(0x8d,addr&255,addr>>8)
    def stx(addr): emit(0x8e,addr&255,addr>>8)
    emit(0x78,0xd8) # SEI CLD
    imm(0xa2,0xff);emit(0x9a) # LDX #FF TXS
    imm(0xa2,0);stx(0x0010);stx(0x0011)
    stx(0x2000);stx(0x2001)
    imm(0xa9,0x40);sta(0x4017) # Disable APU frame IRQ before CLI!
    stx(0xe000) # disable/ack MMC3 IRQ
    imm(0xa9,latch);sta(0xc000)
    sta(0xc001) # request reload
    sta(0xe001) # enable
    imm(0xa9,ctrl);sta(0x2000)
    imm(0xa9,mask);sta(0x2001)
    emit(0x58) # CLI
    target=0x8000+i;emit(0x4c,target&255,target>>8) # spinning mainline
    assert i<0x100
    i=0x100
    imm(0xa9,0x2a);sta(0x0010)
    emit(0xe6,0x11) # INC $11
    sta(0xe000) # IRQ acknowledge
    emit(0x40) # RTI
    b[0x110]=0x40 # NMI: RTI (not MMC3 handler)
    return bytes(b)

def build(tag,mid,sub,ctrl,mask,latch):
    code=make_program(ctrl,mask,latch)
    page=bytearray([0xea]*8192);page[:len(code)]=code
    page[0x1ffa:0x2000]=bytes([0x10,0x81,0x00,0x80,0x00,0x81]) # NMI, RESET, IRQ
    prg=bytes(page)*64
    chr_data=bytes([0]*1024)*512
    h=bytearray(16);h[:4]=b'NES\x1a';h[4]=32;h[5]=64
    h[6]=(mid&15)<<4;h[7]=(mid&0xf0)|0x08;h[8]=(sub<<4)|((mid>>8)&15)
    data=bytes(h)+prg+chr_data
    f=out/('Aurora_IRQ_%s.nes'%tag);f.write_bytes(data)
    return {'filename':f.name,'mapper':mid,'submapper':sub,'w2000':ctrl,'w2001':mask,'latch':latch,'expected_irq':mask!=0 and ctrl in (0x08,0x10),'sha256':hashlib.sha256(data).hexdigest()}
manifest=[]
for name,mid,sub in [('219',219,0),('249',249,0),('4_sub5',4,5),('4_standard',4,0)]:
    for mode,ctrl,mask in [('bg8',0x08,0x08),('sprite8',0x10,0x08),('both_same',0,0x08),('both_hi',0x18,0x08),('render_off',0x08,0x00)]:
        manifest.append(build(name+'_'+mode,mid,sub,ctrl,mask,3))
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Built',len(manifest),'IRQ ROMs')