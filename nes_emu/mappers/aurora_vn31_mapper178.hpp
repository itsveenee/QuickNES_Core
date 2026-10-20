#pragma once
/* Aurora QuickNES / mapper 178, DSOUNDV1/FL-TR8MA educational cartridges.
 * Canonical mapper 178, not variant 551. Mirrors the supplied 8-KiB WRAM
 * under register 3; does not confuse an educational multicart with an
 * exclusively multicart PCB. Source: FCEUmm-next mapper178.c.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
#include <string.h>
class AuroraVn31Mapper178 : public Nes_Mapper {
    uint8_t r[4];
    void sync() {
        const unsigned base=(unsigned(r[1])&7U)|(unsigned(r[2])<<3);
        if(r[0]&2U) {
            set_prg_bank(0x8000,bank_16k,(int)base);
            set_prg_bank(0xc000,bank_16k,(int)(base|((r[0]&4U)?6U:7U)));
        } else if(r[0]&4U) {
            set_prg_bank(0x8000,bank_16k,(int)base);
            set_prg_bank(0xc000,bank_16k,(int)base);
        } else set_prg_bank(0x8000,bank_32k,(int)(base>>1));
        enable_sram();
        if(r[0]&1U)mirror_horiz();else mirror_vert();
        /* Only 8 KiB WRAM is physically allocated for mapper 178. */
    }
public:
    AuroraVn31Mapper178(){register_state(r,sizeof r);}
    virtual void reset_state(){memset(r,0,sizeof r);}
    virtual void apply_mapping(){sync();intercept_writes(0x4800,0x800);}
    virtual bool write_intercepted(nes_time_t,nes_addr_t a,int data){
        if(a<0x4800||a>=0x5000)return false;
        r[a&3]=(uint8_t)data;sync();return true;
    }
    virtual void write(nes_time_t,nes_addr_t,int) {}
};
