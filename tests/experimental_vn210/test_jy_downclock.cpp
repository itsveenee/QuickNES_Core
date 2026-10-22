/* vN210: verify JYASIC IRQ decrement clocks against scalar model and
 * FCEUmm board behavior (on decrement, a prescaler 0 wraps to mask).
 */
#include <cstdio>
#include <cstring>
#include "nes_emu/mappers/aurora_vN118_jyasic_fast.hpp"
static int fails=0,checks=0;
static void verify(bool cond,const char* text) {
    ++checks;
    if(!cond){++fails;std::printf("FAIL %s\n",text);}
}
static AuroraJYASICStage initialize(unsigned mode,unsigned prescaler,unsigned counter) {
    AuroraJYASICStage m;
    m.cpu_write(0xc001,mode);
    m.cpu_write(0xc004,prescaler);
    m.cpu_write(0xc005,counter);
    m.cpu_write(0xc003,1);
    return m;
}
int main() {
    // On downclock, P=0 must wrap on the VERY NEXT clock.
    {
        AuroraJYASICStage a=initialize(0x84,0,0);
        verify(a.cpu_clocks_until_irq()==1U,"downclock P0 C0 predicts 1 cycle");
        a.on_cpu_cycles_fast(1);
        uint8_t s[AuroraJYASICStage::snapshot_size];
        a.save(s);
        verify(a.pending_irq() && s[40]==7 && s[41]==255,
               "downclock P0 C0 clocks on first 0->7 wrap");
    }
    // For P=3 and mask=7, four elapsed clocks lead to first 0->7 wrap.
    {
        AuroraJYASICStage a=initialize(0x84,3,0);
        verify(a.cpu_clocks_until_irq()==4U,"downclock P3 C0 predicts 4 cycles");
        a.on_cpu_cycles_fast(3);
        verify(!a.pending_irq(),"downclock P3 C0 not asserted before cycle 4");
        a.on_cpu_cycles_fast(1);
        verify(a.pending_irq(),"downclock P3 C0 asserted exactly on cycle 4");
    }
    // Exhaustively compare optimized batches to scalar IRQ prescaler clocks.
    const unsigned modes[]={0x40,0x44,0x80,0x84};
    const unsigned ps[]={0,1,3,7,31,255};
    const unsigned cs[]={0,1,127,254,255};
    const unsigned steps[]={0,1,3,7,8,31,256,257,1024};
    for(unsigned i=0;i<sizeof modes/sizeof modes[0];++i)
    for(unsigned j=0;j<sizeof ps/sizeof ps[0];++j)
    for(unsigned k=0;k<sizeof cs/sizeof cs[0];++k)
    for(unsigned l=0;l<sizeof steps/sizeof steps[0];++l) {
        AuroraJYASICStage a=initialize(modes[i],ps[j],cs[k]);
        AuroraJYASICStage b=a;
        a.on_cpu_cycles_fast(steps[l]);
        b.on_cpu_cycles(steps[l]);
        uint8_t x[AuroraJYASICStage::snapshot_size];
        uint8_t y[AuroraJYASICStage::snapshot_size];
        a.save(x);b.save(y);
        verify(std::memcmp(x,y,sizeof x)==0,"fast vs scalar JYASIC state");
    }
    std::printf("JYASIC vN210 fast/scalar %d checks, %d failures\n",checks,fails);
    return fails?1:0;
}
