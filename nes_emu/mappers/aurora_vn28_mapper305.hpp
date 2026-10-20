#pragma once
/* QuickNES / Aurora - NES 2.0 mapper 305, Kaiser KS7031.
 * Dracula II / Noroi no Fuuin FDS cartridge conversion.
 * CPU $6000 has four 2 KiB banked PRG-ROM windows, CPU $8000..FFFF has
 * sixteen fixed 2 KiB banks in reverse order (15..0).  Vertical mirroring.
 * Register index is (address >> 11) & 3; data writes directly select bank.
 * No original FDS expansion audio. Source: FCEUmm-next mapper305.c.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
#include <string.h>

class AuroraVn28Mapper305 : public Nes_Mapper {
    uint8_t bank[4];
public:
    AuroraVn28Mapper305(){register_state(bank,sizeof bank);}
    virtual void reset_state(){memset(bank,0,sizeof bank);}
    virtual void apply_mapping(){
        for(int i=0;i<4;++i)
            set_prg_bank(0x6000+i*0x800,bank_2k,bank[i]);
        for(int i=0;i<16;++i)
            set_prg_bank(0x8000+i*0x800,bank_2k,15-i);
        mirror_vert();
    }
    virtual void write(nes_time_t,nes_addr_t addr,int data){
        const unsigned slot=((unsigned)addr>>11)&3U;
        bank[slot]=(uint8_t)data;
        set_prg_bank(0x6000+slot*0x800,bank_2k,bank[slot]);
    }
};
