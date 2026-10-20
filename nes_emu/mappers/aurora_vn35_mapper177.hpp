#pragma once
/* Mapper 177, standalone oversized BNROM (no bus conflict).
 * FCEUmm-next mapper177.c. PRG32 latch, writable 8K SRAM, CHR RAM.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
class AuroraVn35Mapper177 : public Nes_Mapper {
    uint8_t latch;
    void sync(){
        enable_sram(true);
        set_prg_bank(0x8000,bank_32k,latch&31);
        if(cart().chr_size())set_chr_bank(0,bank_8k,0);
        else set_chr_ram_bank(0,bank_8k,0);
        if(latch&0x20)mirror_horiz(); else mirror_vert();
    }
public:
    AuroraVn35Mapper177(){register_state(&latch,1);}
    virtual void reset_state(){latch=0;}
    virtual void apply_mapping(){sync();}
    virtual void write(nes_time_t,nes_addr_t addr,int data){
        if(addr>=0x8000){latch=(uint8_t)data;sync();}
    }
};
