#!/usr/bin/env python3
"""vN202: original NES2 diagnostic ROMs for mapper 35/209 CHR-RAM protection.

Executes CPU->PPU $2007 writes/readback via the actual QuickNES libretro .so.
Checks protected writes at both ends of pattern memory, nametable integrity,
re-enabling writes, and MAPR/CPU/PPU savestate roundtrip. No commercial ROMs.
"""
import hashlib
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
SMOKE = HERE / "smoke"

class Asm:
    def __init__(self):
        self.data = bytearray()
        self.labels = {}
        self.fixups = []
    def put(self, *b): self.data.extend(bytes(b))
    def mark(self, name):
        assert name not in self.labels
        self.labels[name] = len(self.data)
    def immediate(self, op, val): self.put(op, val & 255)
    def absolute(self, op, addr): self.put(op, addr & 255, addr >> 8)
    def lda(self, val): self.immediate(0xa9, val)
    def sta(self, addr): self.absolute(0x8d, addr)
    def load_store(self, value, addr): self.lda(value); self.sta(addr)
    def jmp(self, label):
        self.put(0x4c, 0, 0)
        self.fixups.append((len(self.data)-2, label))
    def check(self, expected, tag):
        self.immediate(0xc9, expected)  # CMP #imm
        self.put(0xf0, 3)  # BEQ over JMP failure (3 bytes)
        self.jmp("fail")
    def ppu_addr(self, addr):
        self.load_store(addr >> 8, 0x2006)
        self.load_store(addr & 255, 0x2006)
    def ppu_write(self, addr, value):
        self.ppu_addr(addr)
        self.load_store(value, 0x2007)
    def ppu_expect(self, addr, value, tag):
        self.ppu_addr(addr)
        self.absolute(0xad, 0x2007) # buffered read, discarded
        self.absolute(0xad, 0x2007)
        self.check(value, tag)
    def compile(self):
        for off, name in self.fixups:
            where = self.labels[name] + 0x8000
            self.data[off:off+2] = bytes((where & 255, where >> 8))
        assert len(self.data) < 2048
        return bytes(self.data)

def create_program():
    p = Asm()
    p.put(0x78, 0xd8)  # SEI / CLD
    p.load_store(0, 0x2000)
    p.load_store(0, 0x2001)
    p.load_store(0x18, 0xd000) # JY mode: eight 1K CHR banks
    p.load_store(0, 0xd002)    # CHR writes enabled
    p.ppu_write(0x0000, 0x55)
    p.ppu_write(0x1fff, 0x33)
    p.ppu_write(0x2000, 0x26)  # nametable sentinel
    p.ppu_expect(0x0000, 0x55, "unprotected_first")
    p.ppu_expect(0x1fff, 0x33, "unprotected_last")
    p.ppu_expect(0x2000, 0x26, "nametable_before")
    p.load_store(0x40, 0xd002) # protect CHR-RAM only
    p.ppu_write(0x0000, 0xaa)
    p.ppu_write(0x1fff, 0xbb)
    p.ppu_expect(0x0000, 0x55, "protected_first")
    p.ppu_expect(0x1fff, 0x33, "protected_last")
    p.ppu_expect(0x2000, 0x26, "nametable_after")
    p.load_store(0, 0xd002) # unblock
    p.ppu_write(0x0000, 0x3c)
    p.ppu_expect(0x0000, 0x3c, "unprotected_again")
    p.immediate(0xa2, 0x2a) # green
    p.jmp("result")
    p.mark("fail")
    p.immediate(0xa2, 0x16) # red
    p.mark("result")
    p.ppu_addr(0x3f00)
    p.absolute(0x8e, 0x2007) # STX $2007
    p.absolute(0x8e, 0x2007)
    p.mark("stop")
    p.jmp("stop")
    return p.compile()

def generate():
    SMOKE.mkdir(parents=True, exist_ok=True)
    program = create_program()
    made = []
    for mapper in (35, 209):
        prg = bytearray()
        for _ in range(4):
            page = bytearray([0xea]) * 8192
            page[:len(program)] = program
            page[0x1ffa:0x2000] = bytes((0, 0x80, 0, 0x80, 0, 0x80))
            prg.extend(page)
        h = bytearray(16)
        h[:4] = b"NES\\x1a"
        h[4] = 2 # 2 * 16K PRG
        h[5] = 0 # zero CHR-ROM; CHR-RAM expected
        h[6] = (mapper & 15) << 4
        h[7] = (mapper & 0xf0) | 8 # NES2 header
        h[8] = (mapper >> 8) & 15
        h[11] = 7 # NES2 CHR-RAM: 64 << 7 == 8K
        rom = SMOKE / ("Aurora_vN202_JYCHR_%d.nes" % mapper)
        blob = bytes(h) + bytes(prg)
        rom.write_bytes(blob)
        made.append(rom)
        print(rom.name, len(blob), hashlib.sha256(blob).hexdigest(), flush=True)
    return made

def main():
    roms = generate()
    if len(sys.argv) == 1:
        print("Generated ROMs. Pass path to built quicknes_libretro.so to execute.")
        return
    path = Path(sys.argv[1]).resolve()
    sys.path.insert(0, str(ROOT / "tests" / "experimental_vn151" / "tools"))
    from run_actual_quicknes_libretro import Headless
    machine = Headless(path)
    try:
        for rom in roms:
            _, bg0, bg1, pc, mapr = machine.load_and_run(rom, 24)
            print(rom.name, "palette:", hex(bg0), hex(bg1),
                  "PC:", hex(pc), "MAPR:", mapr, flush=True)
            if (bg0, bg1) != (0x2a, 0x2a):
                raise AssertionError("%s: CHR-protect or NT regression: palette %02x/%02x" %
                                     (rom.name, bg0, bg1))
            if mapr != 50: raise AssertionError("Wrong MAPR size")
            if not (0x8000 <= pc < 0x8800): raise AssertionError("Unexpected PC")
    finally:
        machine.close()
    print("PASS: mapper35/209 CHR protection, NT isolation, savestate", flush=True)

if __name__ == "__main__":
    main()
