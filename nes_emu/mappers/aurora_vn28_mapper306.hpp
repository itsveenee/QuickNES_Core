#pragma once
/* QuickNES / Aurora - NES 2.0 mapper 306, Kaiser KS7016 / Exciting Basket
 * FDS conversion; address-decoded register, write DATA ignored.
 * Experimental: FCEUmm reports uncertain original EPROM wiring. No FDS sound.
 * Reference: negativeExponent FCEUmm-next mapper306.c.
 */
#include "Nes_Mapper.h"
#include <stdint.h>

class AuroraVn28Mapper306 : public Nes_Mapper {
    uint8_t prg_bank;
public:
    AuroraVn28Mapper306(){register_state(&prg_bank,1);}
    virtual void reset_state(){prg_bank=0;}
    virtual void apply_mapping(){
        set_prg_bank(0x8000,bank_32k,3);
        set_prg_bank(0x6000,bank_8k,prg_bank);
        // CHR RAM unbanked and mirroring from cart header.
    }
    virtual void write(nes_time_t,nes_addr_t a,int){
        if((a&0xd903)!=0xd903) return;
        prg_bank=(uint8_t)((a&0x40)?((a>>2)&0x0f):(8|((a>>2)&3)));
        set_prg_bank(0x6000,bank_8k,prg_bank);
    }
};
