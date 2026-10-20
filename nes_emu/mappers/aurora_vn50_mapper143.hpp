#pragma once
/* SNESticleAurora QuickNES vN50 / mapper 143, Sachen TC-A01 (NROM).
 * FCEUmm-next mapper143.c: low reads with A8 produce inverted A[5:0],
 * upper bits derive from open bus. This QuickNES core has no tracked full
 * bus latch, so undriven bits follow its existing address-high fallback.
 * No CHR switching, DIP, IRQ or peripherals.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
class AuroraVn50Mapper143 : public Nes_Mapper {
public:
    virtual void apply_mapping(){
        set_prg_bank(0x8000,bank_32k,0);
        set_chr_bank(0,bank_8k,0);
        intercept_reads(0x4100,0x1F00);
    }
    virtual int read(nes_time_t,nes_addr_t addr){
        if(addr<0x4100U || addr>0x5FFFU)return -1;
        const uint8_t bus=(uint8_t)(addr>>8);
        if(addr&0x100U)return (int)((bus&0xC0U)|((~addr)&0x3FU));
        return (int)bus;
    }
    virtual void write(nes_time_t,nes_addr_t,int){}
};
