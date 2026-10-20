#pragma once
/* Aurora QuickNES vN32 / Magic Dragon mapper 107.
 * FCEUmm-next mapper107.c: 32K PRG bank=data>>1; 8K CHR bank=data.
 * Mirroring follows cartridge header.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
class AuroraVn32Mapper107 : public Nes_Mapper {
    uint8_t latch;
    void sync(){
        set_prg_bank(0x8000,bank_32k,latch>>1);
        set_chr_bank(0,bank_8k,latch);
    }
public:
    AuroraVn32Mapper107(){register_state(&latch,1);}
    virtual void reset_state(){latch=0;}
    virtual void apply_mapping(){sync();}
    virtual void write(nes_time_t,nes_addr_t a,int data){
        if(a<0x8000)return;
        latch=(uint8_t)data;sync();
    }
};
