#pragma once
/* QuickNES Aurora vN37 - NTDEC 2718, mapper 557.
 * Single-game Moero TwinBee FDS cartridge conversion with Namco N118
 * register decoder; fixed CHR-RAM, standard 8KiB SRAM and PRG bank control.
 * FCEUmm-next mapper557.c and src/mappers/hw/n118.c (2026).
 */
#include "Nes_Mapper.h"
#include <stdint.h>
#include <string.h>
class AuroraVn37Mapper557 : public Nes_Mapper {
    uint8_t r[9]; // 0..7 N118 regs, 8 command
    void sync(){
        enable_sram();
        set_prg_bank(0x8000,bank_8k,r[6]&15U);
        set_prg_bank(0xa000,bank_8k,r[7]&15U);
        set_prg_bank(0xc000,bank_8k,0xfe);
        set_prg_bank(0xe000,bank_8k,0xff);
        if(cart().chr_size()>0)set_chr_bank(0,bank_8k,0);
        else set_chr_ram_bank(0,bank_8k,0);
        if(r[5]&0x20U)mirror_horiz();else mirror_vert();
    }
public:
    AuroraVn37Mapper557(){register_state(r,sizeof r);}
    virtual void reset_state(){
        static const uint8_t initial[9]={0,2,4,5,6,7,0,1,0};
        memcpy(r,initial,sizeof r);
    }
    virtual void apply_mapping(){sync();}
    virtual void write(nes_time_t,nes_addr_t a,int d){
        if(a<0x8000 || a>=0xa000)return;
        if(a&1)r[r[8]&7U]=(uint8_t)d;
        else r[8]=(uint8_t)d;
        sync();
    }
};
