#!/usr/bin/env python3
"""JYASIC IRQ source=PPU scanline approximation test; NOT physical PPU fetch proof."""
from build_jyasic_roms import Asm,OUT,CHRS
import hashlib,json

def code(positive):
    p=Asm();p.put(0x78,0xd8)
    p.sta_imm(0x40,0x4017);p.sta_imm(0,0x4010)
    p.sta_imm(0,0x0010)
    p.sta_imm(0x10,0x2000) # 8x8 sprite / BG pattern $1000
    p.sta_imm(0x08 if positive else 0x00,0x2001) # BG enabled or disabled
    p.sta_imm(0x45,0xc001) # positive count; A12 source, short prescaler
    p.sta_imm(7,0xc004);p.sta_imm(255,0xc005)
    p.sta_imm(1,0xc003)
    p.put(0x58)
    p.imm(0xa2,0)
    p.mark('wait');p.absolute(0xad,0x0010)
    p.branch(0xd0,'success' if positive else 'fail')
    p.put(0xe8);p.branch(0xd0,'wait')
    p.jmp('fail' if positive else 'success')
    p.mark('success');p.put(0x78);p.imm(0xa2,0x2a);p.jmp('set_color')
    p.mark('fail');p.put(0x78);p.imm(0xa2,0x16)
    p.mark('set_color');p.sta_imm(0,0x2001)
    p.sta_imm(0x3f,0x2006);p.sta_imm(0,0x2006)
    p.absolute(0x8e,0x2007);p.absolute(0x8e,0x2007)
    p.sta_imm(0x08,0x2001)
    p.mark('end');p.jmp('end')
    p.mark('irq');p.put(0xe6,0x10);p.sta_imm(0,0xc002);p.put(0x40)
    raw=p.finalize();return raw,0x8000+p.labels['irq']
manifest={}
for mid in (35,209):
  for variant in ('ppu_a12','ppu_disabled'):
    raw,irq=code(variant=='ppu_a12')
    prg=bytearray()
    for bank in range(64):
      page=bytearray([0xea]*8192)
      page[:len(raw)]=raw
      page[0x1ffa:0x2000]=bytes((irq&255,irq>>8,0,0x80,irq&255,irq>>8));prg+=page
    h=bytearray(16);h[:4]=b'NES\x1a';h[4]=32;h[5]=64;h[6]=(mid&15)<<4;h[7]=(mid&0xf0)|0x08;h[8]=(mid>>8)&15
    data=bytes(h)+bytes(prg)+CHRS
    path=OUT/f'Aurora_JYPPUIRQ_{mid}_{variant}.nes'
    path.write_bytes(data)
    manifest[path.name]={'mapper':mid,'mode':variant,'sha256':hashlib.sha256(data).hexdigest(),
       'scope':'FCEUmm-compatible approximate eight A12 events per scanline; physical fetch not verified'}
(OUT/'jyppu_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Created',len(manifest),'PPU-scanline IRQ diagnostic ROMs')