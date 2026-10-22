/* vN207: hardware-register parity against FCEUmm mapper359.c and 558.c.
 * Pure C++ stage test; checks real register transformations before the
 * libretro/PS2 integration run (does not claim physical PPU clock fidelity).
 */
#include <cstdio>
#include <stdint.h>
#include "nes_emu/mappers/aurora_vn92_mapper540_core_STAGING.hpp"
#include "nes_emu/mappers/aurora_vn112_mapper558_model.hpp"

static int failures=0;
static void check(bool ok, const char* test) {
    std::printf("%s %s\n", ok?"PASS":"FAIL", test);
    if(!ok) ++failures;
}
static void check540() {
    AuroraMapper540Stage m;
    uint8_t state[AuroraMapper540Stage::snapshot_size];
    m.write(0xc001,0x15);
    m.save(state);
    check(state[16]==0,"540 C001 alone does not arm IRQ reload");
    check(state[20]==0x15,"540 C001 stores IRQ high-byte latch");
    m.write(0xc000,0x34);
    m.save(state);
    check(state[16]==1,"540 C000 arms IRQ reload");
    check(state[22]==0x34 && state[23]==0x15,"540 low/high CPU count preserved");
    m.write(0xc002,0x03); // IRQ enable + PPU IRQ clock source
    m.ppu_irq_clock();
    m.save(state);
    check(state[16]==0,"540 PPU IRQ clock consumes reload");
    m.write(0xc001,0x28);
    m.save(state);
    check(state[16]==0 && state[20]==0x28,"540 C001 must not re-arm reload");
    m.write(0xc000,0x45);
    m.save(state);
    check(state[16]==1,"540 C000 reload arms again");
}
static void check558() {
    AuroraMapper558Stage m(false);
    uint8_t state[AuroraMapper558Stage::snapshot_size];
    const unsigned halfMiB=512U*1024U, oneMiB=1024U*1024U;
    m.write(0x5300,0x01,halfMiB);
    m.save(state);
    check(state[3]==0x02,"558 D0/D1 always crossed on $5300");
    m.write(0x5000,0x01,halfMiB);
    m.save(state);
    check(state[0]==0x01,"558 bit1 cancels crossing for $5000");
    m.write(0x5200,0x01,halfMiB);
    m.save(state);
    check(state[2]==0x01,"558 bit1 cancels crossing for $5200 on non-1MiB");
    m.write(0x5200,0x01,oneMiB);
    m.save(state);
    check(state[2]==0x02,"558 1MiB exception leaves $5200 crossed");
    m.write(0x5300,0x02,halfMiB);
    m.save(state);
    check(state[3]==0x01,"558 crossing always applies to $5300, reverse test");
    m.write(0x5000,0x01,halfMiB);
    m.save(state);
    check(state[0]==0x02,"558 crossing $5000 when cancellation bit clear");
    m.write(0x5100,0x01,halfMiB);
    m.save(state);
    check(state[1]==0x02,"558 crossing $5100 when cancellation bit clear");
}
int main(){
    check540();check558();
    std::printf("vN207 stage register tests: %s\n",failures?"FAIL":"PASS");
    return failures?1:0;
}
