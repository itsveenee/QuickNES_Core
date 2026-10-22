#!/usr/bin/env python3
"""Original test ROMs for JYASIC CPU clock (35/209); no commercial data."""
from build_jyasic_roms import Asm,OUT,CHRS
from pathlib import Path
import hashlib,json

def make_program(test):
    p=Asm()
    p.put(0x78,0xd8) # SEI, CLD
    p.sta_imm(0x40,0x4017);p.sta_imm(0,0x4010)
    p.sta_imm(0,0x2000);p.sta_imm(0,0x2001)
    p.sta_imm(0,0x0010)  # IRQ handler count
    if test == 'cpu_cycles':
        p.sta_imm(0x44,0xc001) # source CPU cycles; 8-bit prescaler mode
        p.sta_imm(7,0xc004)
        p.sta_imm(255,0xc005)
    elif test in ('write_low','write_stack','write_alu','no_irq'):
        p.sta_imm(0x47,0xc001) # source all CPU writes; 3-bit prescaler
        p.sta_imm(7,0xc004)
        p.sta_imm(0 if test=='no_irq' else 255,0xc005)
    else:raise AssertionError(test)
    p.sta_imm(1,0xc003) # enable IRQ, do not clear prescaler
    if test == 'write_low':p.sta_imm(0x55,0x0011)
    elif test == 'write_stack':p.imm(0xa9,0x55);p.put(0x48) # PHA
    elif test=='write_alu':p.sta_imm(0x55,0x5802)
    p.put(0x58) # CLI
    p.imm(0xa2,0) # LDX #0
    p.mark('wait');p.absolute(0xad,0x0010) # LDA handler counter
    if test=='no_irq':
        p.branch(0xd0,'fail') # BNE fail
    else:p.branch(0xd0,'success')
    p.put(0xe8) # INX
    p.branch(0xd0,'wait') # BNE wait
    if test=='no_irq': p.jmp('success')
    else: p.jmp('fail')
    p.mark('success');p.put(0x78);p.imm(0xa2,0x2a);p.jmp('color')
    p.mark('fail');p.put(0x78);p.imm(0xa2,0x16)
    p.mark('color');p.sta_imm(0x3f,0x2006);p.sta_imm(0,0x2006)
    p.absolute(0x8e,0x2007);p.absolute(0x8e,0x2007)
    p.sta_imm(0x08,0x2001)
    p.mark('end');p.jmp('end')
    p.mark('irq')
    p.put(0xe6,0x10) # INC $10
    p.sta_imm(0,0xc002) # ACK/disable
    p.put(0x40) # RTI
    program=p.finalize()
    return program,0x8000+p.labels['irq']

records={}
for mid in (35,209):
    for name in ('cpu_cycles','write_low','write_stack','write_alu','no_irq'):
        prog,irq=make_program(name)
        prg=bytearray()
        for bank in range(64):
            page=bytearray([0xea]*8192)
            page[:len(prog)]=prog
            # Last vectors: NMI and IRQ point to handler, RESET at $8000
            page[0x1ffa:0x2000]=bytes((irq&255,irq>>8,0,0x80,irq&255,irq>>8))
            prg.extend(page)
        h=bytearray(16);h[:4]=b'NES\x1a';h[4]=32;h[5]=64
        h[6]=(mid&0x0f)<<4;h[7]=(mid&0xf0)|0x08;h[8]=(mid>>8)&15
        data=bytes(h)+bytes(prg)+CHRS
        path=OUT/f'Aurora_JYIRQ_{mid}_{name}.nes';path.write_bytes(data)
        records[path.name]={'mapper':mid,'irq_test':name,'IRQ_handler':hex(irq),'sha256':hashlib.sha256(data).hexdigest(),
             'expected_palette':'2a','rom_bytes':len(data)}
(OUT/'jyirq_manifest.json').write_text(json.dumps(records,indent=2)+'\n')
print('Generated',len(records),'JYASIC CPU/write IRQ self-test ROMs')