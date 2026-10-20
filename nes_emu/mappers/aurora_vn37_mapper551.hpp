#pragma once
/* QuickNES Aurora vN37 - mapper 551, DSOUNDV1/FL-TR8MA variant.
 * Distinct from 178: fixed WRAM bank, CHR-ROM register and hardwired
 * cartridge header mirroring. FCEUmm-next mapper178.c, mapper551 variant.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
#include <string.h>
class AuroraVn37Mapper551 : public Nes_Mapper {
    uint8_t r[4];
    void sync(){
        const unsigned base=(unsigned(r[1])&7U)|(unsigned(r[2])<<3);
        if(r[0]&2U){
            set_prg_bank(0x8000,bank_16k,(int)base);
            set_prg_bank(0xc000,bank_16k,(int)(base|((r[0]&4U)?6U:7U)));
        }else if(r[0]&4U){
            set_prg_bank(0x8000,bank_16k,(int)base);
            set_prg_bank(0xc000,bank_16k,(int)base);
        }else set_prg_bank(0x8000,bank_32k,(int)(base>>1));
        enable_sram();
        if(cart().chr_size()>0)set_chr_bank(0,bank_8k,r[3]);
        // Unlike mapper 178, no writable mirror control on 551.
    }
public:
    AuroraVn37Mapper551(){register_state(r,sizeof r);}
    virtual void reset_state(){memset(r,0,sizeof r);}
    virtual void apply_mapping(){sync();intercept_writes(0x4800,0x800);}
    virtual bool write_intercepted(nes_time_t,nes_addr_t a,int d){
        if(a<0x4800 || a>=0x5000)return false;
        r[a&3]=(uint8_t)d;sync();return true;
    }
    virtual void write(nes_time_t,nes_addr_t,int){}
};
