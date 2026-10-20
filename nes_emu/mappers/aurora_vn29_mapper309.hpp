#pragma once
/* SNESticleAurora / QuickNES — reconstructed mapper 309, LH51.
 * Ai Senshi Nicol FDS cart conversion, PRG-RAM $6000; $8000 8K switchable;
 * banks FD,FE,FF fixed.  Preserve cart-header mirroring until $F000 write.
 * Reference: negativeExponent FCEUmm-next mapper309.c.
 * Omitted FDS expansion sound; no ROM gameplay validation.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
class AuroraVn29Mapper309 : public Nes_Mapper {
    uint8_t s[3]; // PRG bank, mirroring register, mirroring-written flag
public:
    AuroraVn29Mapper309(){register_state(s,sizeof s);}
    virtual void reset_state(){s[0]=s[1]=s[2]=0;}
    virtual void apply_mapping(){
        enable_sram();
        set_prg_bank(0x8000,bank_8k,s[0]);
        set_prg_bank(0xa000,bank_8k,0xfd);
        set_prg_bank(0xc000,bank_8k,0xfe);
        set_prg_bank(0xe000,bank_8k,0xff);
        if(s[2]) { if(s[1]&8)mirror_horiz();else mirror_vert(); }
    }
    virtual void write(nes_time_t,nes_addr_t addr,int data){
        if(addr>=0x8000 && addr<=0x8fff) {
            s[0]=(uint8_t)data;
            set_prg_bank(0x8000,bank_8k,s[0]);
        } else if(addr>=0xf000) {
            s[1]=(uint8_t)data;s[2]=1;
            if(s[1]&8)mirror_horiz();else mirror_vert();
        }
    }
};
