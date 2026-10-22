/* vN211: physical PPU address sequence MODEL regression for 272 and 558.
 * Intentional boundary: this tests models with supplied PPU bus addresses.
 * It does NOT claim the QuickNES renderer supplies cycle-accurate PPU fetches.
 */
#include <cstdio>
#include <stdint.h>
#include "nes_emu/mappers/aurora_vn90_mapper272_model.hpp"
#include "nes_emu/mappers/aurora_vn112_mapper558_model.hpp"

static int fails=0, checks=0;
static void check(bool cond,const char* s) {
    ++checks; if(!cond){++fails;std::printf("FAIL %s\n",s);}
}
static void falling_pa13(AuroraMapper272Stage& m) {
    m.ppu_fetch(0x2000);m.ppu_fetch(0x0000);
}
static void test272() {
    AuroraMapper272Stage m;
    m.write(0xc008,1); // PAL physical IRQ enable
    for(int i=0;i<83;++i)falling_pa13(m);
    check(!m.pending_irq() && m.counter()==83U,"272 first 83 physical PA13 falls do not assert IRQ");
    falling_pa13(m);
    check(m.pending_irq() && m.counter()==0U,"272 fall #84 asserts and wraps counter");
    m.write(0xc004,0); // acknowledge
    check(!m.pending_irq(),"272 direct PAL IRQ acknowledge clears line");
    m.write(0x800c,1); // direct IRQ
    check(m.pending_irq(),"272 direct PAL force IRQ independent of physical fetch");
    m.write(0xc00c,0);
    check(!m.pending_irq() && m.counter()==0,"272 PAL disable clears IRQ/counter");
    AuroraMapper272Stage resume;
    resume.write(0xc008,1);
    for(int i=0;i<42;++i)falling_pa13(resume);
    uint8_t snapshot[AuroraMapper272Stage::snapshot_size];
    resume.save(snapshot);
    AuroraMapper272Stage restored;restored.load(snapshot);
    check(restored.counter()==42,"272 partial physical scanline count survives save");
    for(int i=0;i<41;++i)falling_pa13(restored);
    check(!restored.pending_irq(),"272 loaded state has no early IRQ");
    falling_pa13(restored);
    check(restored.pending_irq(),"272 restored physical scanline count asserts at #84");
}
static void test558() {
    AuroraMapper558Stage m(false);
    AuroraMapper558Stage::Mapping bank;
    m.write(0x5000,0x80,512*1024); // 1bpp PA13/PA09 latch enable
    m.ppu_fetch(0x0200);
    m.ppu_fetch(0x2200); // rising PA13 with PA09 set
    m.map(bank);
    check(bank.chr4[0]==1 && bank.chr4[1]==1,"558 rising PA13 captures PA09=1");
    m.ppu_fetch(0x2000); // PA13 remains high; must NOT resample
    m.map(bank);
    check(bank.chr4[0]==1,"558 no PA09 resample while PA13 high");
    m.ppu_fetch(0x0000); // PA13 falls
    m.ppu_fetch(0x2000); // rising PA13 PA09=0
    m.map(bank);
    check(bank.chr4[0]==0,"558 next rising PA13 samples PA09=0");
    uint8_t snapshot[AuroraMapper558Stage::snapshot_size];
    m.save(snapshot);
    AuroraMapper558Stage restored(false);
    restored.load(snapshot);restored.map(bank);
    check(bank.chr4[0]==0,"558 PPU latch state survives save/load");
}
int main() {
    test272();test558();
    std::printf("vN211 physical-address model: %d checks %d failures (model ONLY)\n",checks,fails);
    return fails?1:0;
}
