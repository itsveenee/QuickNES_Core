#!/usr/bin/env python3
"""Original JYASIC nametable-to-CHR-ROM and CIRAM smoke test ROMs."""
from build_jyasic_roms import Asm, OUT, CHRS
import json,hashlib

def program():
    p=Asm();p.put(0x78,0xd8)
    p.sta_imm(0x40,0x4017);p.sta_imm(0,0x4010)
    p.sta_imm(0,0x2000);p.sta_imm(0,0x2001)
    # Enable extended nametable and force CHR-ROM source ($D000 bit5+6).
    p.sta_imm(0x60,0xd000)
    p.sta_imm(0x13,0xb000);p.sta_imm(0,0xb004)
    def read_nt(v,tag):
        p.sta_imm(0x20,0x2006);p.sta_imm(0x80,0x2006)
        p.absolute(0xad,0x2007);p.absolute(0xad,0x2007)
        p.imm(0xc9,v);p.branch(0xf0,'ok_'+tag);p.jmp('fail');p.mark('ok_'+tag)
    read_nt(0x13,'chr_nt_13')
    p.sta_imm(0x21,0xb000)
    read_nt(0x21,'chr_nt_21')
    # Disable extended nametable; switch to CIRAM, test write/read.
    p.sta_imm(0,0xd000)
    p.sta_imm(0x20,0x2006);p.sta_imm(0x80,0x2006)
    p.sta_imm(0x5a,0x2007)
    read_nt(0x5a,'ciram_echo')
    p.imm(0xa2,0x2a);p.jmp('color');p.mark('fail');p.imm(0xa2,0x16)
    p.mark('color');p.sta_imm(0x3f,0x2006);p.sta_imm(0,0x2006)
    p.absolute(0x8e,0x2007);p.absolute(0x8e,0x2007)
    p.sta_imm(0x08,0x2001);p.mark('end');p.jmp('end')
    return p.finalize()
P=program(); records={}
for mid in (35,209):
    prg=bytearray()
    for bank in range(64):
        page=bytearray([0xea]*8192);page[:len(P)]=P
        page[0x1ffa:0x2000]=bytes([0,0x80,0,0x80,0,0x80]);prg+=page
    h=bytearray(16);h[:4]=b'NES\x1a';h[4]=32;h[5]=64;h[6]=(mid&15)<<4;h[7]=(mid&0xf0)|0x08;h[8]=(mid>>8)&15
    blob=bytes(h)+bytes(prg)+CHRS
    path=OUT/f'Aurora_JYNT_{mid}_CHR_ROM_CIRAM.nes';path.write_bytes(blob)
    records[path.name]={'sha256':hashlib.sha256(blob).hexdigest(),'mapper':mid,'reads_from_nt_chr_rom':2,'CIRAM_echo':1}
(OUT/'jynt_manifest.json').write_text(json.dumps(records,indent=2)+'\n')
print('Built',len(records),'JYASIC nametable-source smoke ROMs')