#pragma once
/* SNESticleAurora / QuickNES — reconstructed mapper 538, 60-1064-16L.
 * Super Soccer Champion (FDS Exciting Soccer cartridge conversion).
 * NESdev canonical bank table, PRG $6000 and $8000 switchable, upper fixed
 * 13,14,15.  Writes $C000-$DFFF; vertical mirroring, 8K CHR RAM.
 * Excludes a misordered-ROM variant (FCEUmm CRC A8C6D77D) until an exact
 * reliable CRC policy can be independently verified. FDS sound unavailable.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
class AuroraVn29Mapper538 : public Nes_Mapper {
    uint8_t s[1];
public:
    AuroraVn29Mapper538(){register_state(s,sizeof s);}
    virtual void reset_state(){s[0]=0;}
    virtual void apply_mapping(){
        const unsigned v=s[0]&15U;
        const unsigned bank6=v|1U;
        const unsigned bank8=(v&1U)&&!(v&8U)?10U:v&~1U;
        set_prg_bank(0x6000,bank_8k,bank6);
        set_prg_bank(0x8000,bank_8k,bank8);
        set_prg_bank(0xa000,bank_8k,13);
        set_prg_bank(0xc000,bank_8k,14);
        set_prg_bank(0xe000,bank_8k,15);
        mirror_vert();
    }
    virtual void write(nes_time_t,nes_addr_t addr,int data){
        if(addr<0xc000 || addr>0xdfff)return;
        s[0]=(uint8_t)(data&15);
        const unsigned v=s[0];
        set_prg_bank(0x6000,bank_8k,v|1U);
        set_prg_bank(0x8000,bank_8k,((v&1U)&&!(v&8U))?10U:(v&~1U));
    }
};
