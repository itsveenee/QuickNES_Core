#pragma once
/* SPDX-License-Identifier: GPL-2.0-or-later
 * QuickNES adaptation of FCEUmm-next mapper038.c.
 * Crime Busters (Bit Corp.) standalone cartridge; writes $7000-$7FFF.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
class AuroraVn34Mapper038 : public Nes_Mapper {
    uint8_t latch;
    void sync() {
        set_prg_bank(0x8000,bank_32k,latch&3);
        if(cart().chr_size()>0)set_chr_bank(0,bank_8k,(latch>>2)&3);
    }
public:
    AuroraVn34Mapper038(){register_state(&latch,1);}
    virtual void reset_state(){latch=0;}
    virtual void apply_mapping(){sync();intercept_writes(0x7000,0x1000);}
    virtual bool write_intercepted(nes_time_t,nes_addr_t a,int d){
        if(a<0x7000||a>=0x8000)return false;
        latch=uint8_t(d);sync();return true;
    }
    virtual void write(nes_time_t,nes_addr_t,int){}
};
