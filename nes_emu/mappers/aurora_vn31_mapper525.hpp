#pragma once
/* Aurora QuickNES / mapper 525, KS7021A (Contra/Getsu Fuma Den bootleg).
 * Banking and registers cross-checked with FCEUmm-next mapper525.c.
 * C++98, no CPU/PPU global patches.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
#include <string.h>
class AuroraVn31Mapper525 : public Nes_Mapper {
    uint8_t r[10]; /* [0]=PRG, [1]=mirror, [2..9]=CHR 1 KiB */
    void sync_prg() { set_prg_bank(0x8000,bank_16k,r[0]>>1);set_prg_bank(0xc000,bank_16k,last_bank); }
    void sync_chr() { for(int i=0;i<8;++i) set_chr_bank(i*0x400,bank_1k,r[i+2]); }
    void sync_mirror() { if(r[1]&1)mirror_horiz();else mirror_vert(); }
public:
    AuroraVn31Mapper525() { register_state(r,sizeof r); }
    virtual void reset_state() { memset(r,0,sizeof r); }
    virtual void apply_mapping() { sync_prg();sync_chr();sync_mirror(); }
    virtual void write(nes_time_t,nes_addr_t a,int data) {
        if(a>=0x8000&&a<0x9000){r[0]=(uint8_t)data;sync_prg();}
        else if(a>=0x9000&&a<0xa000){r[1]=(uint8_t)data;sync_mirror();}
        else if(a>=0xb000&&a<0xc000){
            const unsigned idx=(unsigned)a&7U;
            r[idx+2]=(uint8_t)data;
            set_chr_bank(idx*0x400,bank_1k,r[idx+2]);
        }
    }
};
