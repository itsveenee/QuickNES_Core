#pragma once
/* Aurora QuickNES vN32 / Sachen SA-72007 mapper 145.
 * FCEUmm-next mapper145.c: writes $4100-$5FFF (A8 high), CHR=data>>7.
 * The PRG bank is hard-wired to 0, no bus conflict on this board.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
class AuroraVn32Mapper145 : public Nes_Mapper {
    uint8_t r;
    void sync(){set_prg_bank(0x8000,bank_32k,0);
                set_chr_bank(0,bank_8k,r>>7);}
public:
    AuroraVn32Mapper145(){register_state(&r,1);}
    virtual void reset_state(){r=0;}
    virtual void apply_mapping(){sync();intercept_writes(0x4100,0x1f00);}
    virtual bool write_intercepted(nes_time_t,nes_addr_t a,int d){
        if(a<0x4100||a>0x5fff||!(a&0x0100))return false;
        r=(uint8_t)d;set_chr_bank(0,bank_8k,r>>7);return true;
    }
    virtual void write(nes_time_t,nes_addr_t,int){}
};
