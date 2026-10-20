#pragma once
/* Aurora QuickNES vN32 / NTDEC-ASDER mapper 112.
 * FCEUmm-next mapper112.c: PRG 8K slots at $8000/$A000, last 16K
 * fixed at $C000; CHR 2K slots from reg2/3 >> 1, upper 1K slots with
 * per-slot 9th CHR bank bit; default banks 0,1,0,2,4,5,6,7.
 * RAM storage uses QuickNES's existing 8K SRAM window; no new globals.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
#include <string.h>
class AuroraVn32Mapper112 : public Nes_Mapper {
    uint8_t r[11]; // 0..7 bank regs; 8 command; 9 mirror; 10 chrBase.
    void prg(){
        set_prg_bank(0x8000,bank_8k,r[0]);
        set_prg_bank(0xa000,bank_8k,r[1]);
        set_prg_bank(0xc000,bank_16k,last_bank);
    }
    void chr(){
        set_chr_bank(0x0000,bank_2k,r[2]>>1);
        set_chr_bank(0x0800,bank_2k,r[3]>>1);
        for(int i=0;i<4;i++) {
            const unsigned high=(r[10]&(0x10U<<i))?0x100U:0U;
            set_chr_bank(0x1000+i*0x400,bank_1k,r[4+i]|high);
        }
    }
    void mirror(){if(r[9]&1)mirror_horiz();else mirror_vert();}
public:
    AuroraVn32Mapper112(){register_state(r,sizeof r);}
    virtual void reset_state(){
        memset(r,0,sizeof r);
        r[1]=1;r[3]=2;r[4]=4;r[5]=5;r[6]=6;r[7]=7;
    }
    virtual void apply_mapping(){if(cart().has_battery_ram())enable_sram();prg();chr();mirror();}
    virtual void write(nes_time_t,nes_addr_t a,int data){
        if(a<0x8000)return;
        const uint8_t d=(uint8_t)data;
        switch(a&0xe000) {
        case 0x8000:r[8]=d;break;
        case 0xa000:r[r[8]&7]=d;prg();chr();break;
        case 0xc000:r[10]=d;chr();break;
        case 0xe000:r[9]=d;mirror();break;
        }
    }
};
