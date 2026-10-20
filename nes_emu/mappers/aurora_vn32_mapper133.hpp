#pragma once
/* Aurora QuickNES vN32 / Sachen 3009 mapper 133.
 * FCEUmm-next mapper133.c: writes $4100-$5FFF only when A8 is set.
 * PRG=(data>>2)&1, CHR=data&3; header mirroring kept.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
class AuroraVn32Mapper133 : public Nes_Mapper {
    uint8_t r;
    void sync(){set_prg_bank(0x8000,bank_32k,(r>>2)&1);
                set_chr_bank(0,bank_8k,r&3);}
public:
    AuroraVn32Mapper133(){register_state(&r,1);}
    virtual void reset_state(){r=0;}
    virtual void apply_mapping(){sync();intercept_writes(0x4100,0x1f00);}
    virtual bool write_intercepted(nes_time_t,nes_addr_t a,int d){
        if(a<0x4100||a>0x5fff||!(a&0x0100))return false;
        r=(uint8_t)d;sync();return true;
    }
    virtual void write(nes_time_t,nes_addr_t,int){}
};
