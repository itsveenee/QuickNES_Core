#pragma once
/* Aurora QuickNES vN32 / NES 2.0 29 RET-CUFROM.
 * Reference: FCEUmm-next mapper029.c. 3 PRG select bits, 2 CHR-RAM
 * select bits, 32KiB physical CHR RAM, 8KiB SRAM. ROM header mirroring kept.
 * Flash programming, if present, is a separate peripheral feature.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
class AuroraVn32Mapper029 : public Nes_Mapper {
    uint8_t latch;
    void sync() {
        set_prg_bank(0x8000,bank_16k,(latch>>2)&7);
        set_prg_bank(0xc000,bank_16k,last_bank);
        set_chr_ram_bank(0,bank_8k,latch&3);
    }
public:
    AuroraVn32Mapper029(){register_state(&latch,1);}
    virtual long chr_ram_size() const {return 0x8000;}
    virtual void reset_state(){latch=0;}
    virtual void apply_mapping(){enable_sram();sync();}
    virtual void write(nes_time_t,nes_addr_t a,int data){
        if(a<0x8000)return;
        latch=(uint8_t)data;
        sync();
    }
};
