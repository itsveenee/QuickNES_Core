#pragma once
/* Aurora vN42 mapper 242, UNL-43272 (single-game Waixing RPG PCB).
 * Hardware reference: FCEUmm-next mapper242.c (negativeExponent, GPL 2+).
 * Address latch, PRG ROM bank modes, unbanked 8KiB CHR-RAM and PRG-RAM.
 * Also models dual PRG chip bank math, but its multicart-only DIP switch
 * and CHR-RAM write-protect behavior are outside the intended game scope.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
class AuroraVn42Mapper242 : public Nes_Mapper {
    uint16_t address;
    void sync() {
        const unsigned a=(unsigned)address;
        unsigned prg=(a>>2)&0x1fU;
        const unsigned a14=a&1U;
        const bool nrom=((a>>7)&1U)!=0;
        const bool unrom=((a>>9)&1U)!=0;
        const unsigned long rom=(unsigned long)cart().prg_size();
        const bool two_chips=(rom>0x20000UL)&&(rom&0x20000UL);
        if(two_chips) {
            const unsigned count=(unsigned)(rom>>14);
            const unsigned first_count=count&~8U;
            if(a&0x600U)prg&=(first_count-1U);
            else prg=(prg&7U)+first_count;
        }
        const unsigned bank0=prg&~a14;
        const unsigned bank1=((prg|a14)&~(7U*(!nrom)*(!unrom)))
                                 |(7U*(!nrom)*(unrom));
        set_prg_bank(0x8000,bank_16k,(int)bank0);
        set_prg_bank(0xc000,bank_16k,(int)bank1);
        if(a&2U)mirror_vert();else mirror_horiz();
    }
public:
    AuroraVn42Mapper242():address(0) { register_state(&address,sizeof address); }
    virtual void reset_state() { address=0; }
    virtual void apply_mapping() {
        enable_sram(true);
        if(cart().chr_size())set_chr_bank(0,bank_8k,0);
        else set_chr_ram_bank(0,bank_8k,0);
        sync();
    }
    virtual void write(nes_time_t,nes_addr_t a,int) {
        if(a<0x8000 || a>0xffff || address==(uint16_t)a)return;
        address=(uint16_t)a;sync();
    }
};
