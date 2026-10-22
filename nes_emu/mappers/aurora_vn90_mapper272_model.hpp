#pragma once
/* Aurora QuickNES vN57 mapper 272 (Akumajou Special: Boku Dracula-kun)
 * Pure ASIC model, STAGING ONLY. Only exact PPU fetch address events may clock
 * IRQ; never substitute $2006 writes or a scanline approximation.
 * Behavioral reference: libretro-fceumm_next/src/mappers/mapper272.c,
 * GPL-2.0-or-later. Keep applicable license notices on integration.
 */
#include <stdint.h>
#include <string.h>
class AuroraMapper272Stage {
public:
    enum { snapshot_size=16 };
    struct Mapping {unsigned prg8[4],chr1[8];unsigned nt[4];};
private:
    uint8_t prg[2], chr[8], mirror_hv, one_screen, irq_count, irq_active;
    uint8_t irq_pending, last_ppu_2xxx;
public:
    AuroraMapper272Stage() {reset();}
    void reset() {
        memset(prg,0,sizeof prg);memset(chr,0,sizeof chr);
        mirror_hv=0;one_screen=0;irq_count=0;irq_active=0;
        irq_pending=0;last_ppu_2xxx=0;
    }
    bool write(unsigned addr,unsigned value) {
        addr&=0xffffU;value&=255U;
        if(addr<0x8000U)return false;
        const unsigned region=addr&0xf000U;
        if(region==0x8000U){prg[0]=(uint8_t)value;}
        else if(region==0x9000U)mirror_hv=(uint8_t)value;
        else if(region==0xa000U)prg[1]=(uint8_t)value;
        else if(region>=0xb000U && region<=0xe000U){
            const unsigned slot=(((addr-0xb000U)>>11)&6U)|((addr>>1)&1U);
            if(addr&1U)chr[slot]=(uint8_t)((chr[slot]&15U)|((value<<4)&240U));
            else chr[slot]=(uint8_t)((chr[slot]&240U)|(value&15U));
        }
        switch(addr&0xc00cU){
        case 0x8004U:one_screen=(uint8_t)value;break;
        case 0x800cU:irq_pending=1;break; // PAL direct IRQ assert
        case 0xc004U:irq_pending=0;break;
        case 0xc008U:irq_active=1;break;
        case 0xc00cU:irq_active=0;irq_count=0;irq_pending=0;break;
        }
        return true;
    }
    void ppu_fetch(unsigned addr) {
        const unsigned a=(addr>>13)&1U;
        if(last_ppu_2xxx && !a && irq_active) {
            if(++irq_count==84U){irq_count=0;irq_pending=1;}
        }
        last_ppu_2xxx=(uint8_t)a;
    }
    bool pending_irq()const{return irq_pending!=0;}
    unsigned counter()const{return irq_count;}
    void map(Mapping &out,unsigned num_prg8) const {
        out.prg8[0]=prg[0];out.prg8[1]=prg[1];
        // FCEUmm setprg16(0xC000, ~0): last 16 KiB pair, not #0.
        out.prg8[2]=num_prg8>=2U?num_prg8-2U:0U;
        out.prg8[3]=num_prg8>=1U?num_prg8-1U:0U;
        for(unsigned i=0;i<8U;++i)out.chr1[i]=chr[i];
        if(one_screen&2U) {
            for(unsigned i=0;i<4U;++i)out.nt[i]=one_screen&1U;
        } else if((mirror_hv&1U)==0U) {
            // FCEUmm SyncMirror: setmirror((mirrorHV & 1) ^ 1).
            // MI_V=1 for 0, MI_H=0 for 1. The earlier polarity was inverted.
            out.nt[0]=0;out.nt[1]=1;out.nt[2]=0;out.nt[3]=1;
        } else {
            out.nt[0]=0;out.nt[1]=0;out.nt[2]=1;out.nt[3]=1;
        }
    }
    void save(uint8_t b[snapshot_size]) const {
        b[0]=prg[0];b[1]=prg[1];
        memcpy(b+2,chr,8);
        b[10]=mirror_hv;b[11]=one_screen;b[12]=irq_count;
        b[13]=irq_active;b[14]=irq_pending;b[15]=last_ppu_2xxx;
    }
    void load(const uint8_t b[snapshot_size]) {
        prg[0]=b[0];prg[1]=b[1];memcpy(chr,b+2,8);
        mirror_hv=b[10];one_screen=b[11];irq_count=b[12];
        irq_active=b[13]&1U;irq_pending=b[14]&1U;
        last_ppu_2xxx=b[15]&1U;
    }
};