#pragma once
/* SNESticleAurora / QuickNES — reconstructed mapper 312, KS7013B.
 * $6000-$7FFF write selects PRG16 at $8000; $C000 last PRG16 fixed.
 * High ROM writes set mirroring latch, bit0:0 vertical/1 horizontal.
 * Compare negativeExponent FCEUmm-next mapper312.c and latch infrastructure.
 * No ROM compatibility test. Different from KS7013 (without 'B').
 */
#include "Nes_Mapper.h"
#include <stdint.h>
class AuroraVn29Mapper312 : public Nes_Mapper {
    uint8_t s[3]; // PRG, mirroring bit, has explicit mirroring latch
public:
    AuroraVn29Mapper312(){register_state(s,sizeof s);}
    virtual void reset_state(){s[0]=s[1]=0;s[2]=1;}
    virtual void apply_mapping(){
        set_prg_bank(0x8000,bank_16k,s[0]);
        set_prg_bank(0xc000,bank_16k,last_bank);
        if(s[2]){if(s[1]&1)mirror_horiz();else mirror_vert();}
        intercept_writes(0x6000,0x2000);
    }
    virtual bool write_intercepted(nes_time_t,nes_addr_t a,int v){
        if(a<0x6000||a>0x7fff)return false;
        s[0]=(uint8_t)v;
        set_prg_bank(0x8000,bank_16k,s[0]);
        return true;
    }
    virtual void write(nes_time_t,nes_addr_t,int v){
        s[1]=(uint8_t)v;s[2]=1;
        if(s[1]&1)mirror_horiz();else mirror_vert();
    }
};
