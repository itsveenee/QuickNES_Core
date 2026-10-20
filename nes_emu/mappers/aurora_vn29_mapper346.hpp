#pragma once
/* SNESticleAurora / QuickNES — reconstructed mapper 346, KS7012 Zanac.
 * Reset PRG32 bank 1. Writes at ONLY $E0A0/$EE36 select banks 0/1.
 * 8 KiB cart SRAM $6000, CHR RAM follows base mapper initialization.
 * Reference: negativeExponent FCEUmm-next mapper346.c.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
class AuroraVn29Mapper346 : public Nes_Mapper {
    uint8_t s[1];
public:
    AuroraVn29Mapper346(){register_state(s,sizeof s);}
    virtual void reset_state(){s[0]=1;}
    virtual void apply_mapping(){enable_sram();set_prg_bank(0x8000,bank_32k,s[0]);}
    virtual void write(nes_time_t,nes_addr_t a,int){
        if(a==0xe0a0){s[0]=0;set_prg_bank(0x8000,bank_32k,0);}
        else if(a==0xee36){s[0]=1;set_prg_bank(0x8000,bank_32k,1);}
    }
};
