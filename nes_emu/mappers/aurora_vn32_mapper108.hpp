#pragma once
/* Aurora QuickNES vN32 / mapper 108 (single-game FDS cartridge conversions).
 * FCEUmm-next mapper108.c: Bubble Bobble, Falsion, Dababa, Pro Wrestling.
 * Supports NES2 submappers 1..4 and legacy header heuristic. FDS additional
 * audio is not implemented in QuickNES; mapper banking is independent.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
class AuroraVn32Mapper108 : public Nes_Mapper {
    uint8_t reg;
    unsigned variant() const {
        const unsigned sm=(unsigned)cart().submapper_code();
        if(sm>=1 && sm<=4)return sm;
        if(cart().chr_size()==0)return (cart().mirroring()&1)?3:1;
        return cart().chr_size()<=32768?4:2;
    }
    void sync(){
        set_prg_bank(0x6000,bank_8k,variant()==4?last_bank:(int)reg);
        set_prg_bank(0x8000,bank_32k,last_bank);
        if(cart().chr_size()>0)set_chr_bank(0,bank_8k,reg);
    }
public:
    AuroraVn32Mapper108(){register_state(&reg,1);}
    virtual void reset_state(){reg=0;}
    virtual void apply_mapping(){sync();}
    virtual void write(nes_time_t,nes_addr_t a,int data){
        unsigned sm=variant();
        if(sm==1 && a<0xf000)return;
        if(sm==2 && a<0xe000)return;
        if(a<0x8000)return;
        reg=(uint8_t)data;sync();
    }
};
