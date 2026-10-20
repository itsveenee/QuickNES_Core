#pragma once
/* Mapper 263, UNL-KOF97 MMC3 register address/data permutation.
 * FCEUmm-next mapper263.c: preserve native QuickNES MMC3 IRQ/PPU clock.
 */
#include "mapper004.hpp"
class AuroraVn35Mapper263 : public Mapper004 {
public:
    virtual void write(nes_time_t time,nes_addr_t a,int d){
        const nes_addr_t addr=(a&0xe000)|((a>>12)&1);
        const unsigned v=(unsigned)d&255U;
        const int data=int((v&0xd8U)|((v&4U)<<3)|((v&1U)<<2)|
            ((v&0x20U)>>4)|((v&2U)>>1));
        Mapper004::write(time,addr,data);
    }
};
