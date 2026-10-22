#!/usr/bin/env python3
"""vN166: 93C66 EWEN/WRITE/READ + distinct WRAM test, original 6502 ROM."""
from pathlib import Path
import sys,json,hashlib
sys.path.insert(0,str(Path(__file__).parent))
from build_seven_new_roms import A,header,initialize,OUT

def program():
 p=A(0x8000);initialize(p)
 # Disable scrambled D0/D1 wires for $5200 serial data.
 p.sta(2,0x5300)
 def bit(v):
  p.sta(4 | v,0x5200)
  p.sta(6 | v,0x5200)
 def stream(op,addr):
  p.sta(0,0x5200) # lower CS resets serial state
  bit(1)
  for v in ((op>>1)&1,op&1):bit(v)
  for i in range(8,-1,-1):bit((addr>>i)&1)
 # EWEN: opcode 00, address bit8..7=11 -> 0x180
 stream(0,0x180);p.sta(0,0x5200)
 stream(1,5)
 for i in range(7,-1,-1):bit((0xA5>>i)&1)
 p.sta(0,0x5200)
 # The EEPROM must not overlap volatile WRAM window $6000.
 p.sta(0x6e,0x6000);p.check(0x6000,0x6e)
 # Begin READ at address 5; clock first two bits = 1,0 for 0xA5.
 stream(2,5)
 bit(0);p.check(0x5000,4)
 bit(0);p.check(0x5000,0)
 p.finish()
 return p.code()
prog=program();prg=bytearray(16*32768)
for b in range(16):
 o=b*32768;prg[o:o+len(prog)]=prog
 for vec in (0x7ffa,0x7ffc,0x7ffe):prg[o+vec:o+vec+2]=(0x8000).to_bytes(2,'little')
h=bytearray(header(558,len(prg),8192));h[10]=0x30 # NES2 byte10: 3<<4 = 512 byte PRG NVRAM
rom=bytes(h)+prg+bytes(8192)
fn=OUT/'Aurora_vN166_mapper558_EEPROM_EWEN_WRITE_READ_WRAM.nes';fn.write_bytes(rom)
meta={'mapper':558,'rom_sha256':hashlib.sha256(rom).hexdigest(),'prg_nvram_byte_10':h[10], 'expected_battery_bytes':512,'expected_eeprom_addr':5,'expected_eeprom_value':165,'expected_color':42,'expected_wram_6000':110, 'features':['EWEN','WRITE','READ','separate WRAM']}
(OUT/'eeprom_manifest.json').write_text(json.dumps(meta,indent=2)+'\n')
print('Wrote',fn,len(rom),'bytes; 6502 program',len(prog),'bytes')