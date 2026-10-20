#pragma once
/* Aurora vN42 mapper 227, address-latch PCB, including single-game FW-01.
 * Hardware source: FCEUmm-next mapper227.c (negativeExponent, GNU GPL 2+).
 * Address (not data) selects PRG banks and mirroring. PRG-RAM is 8 KiB.
 * Covers unprotected CHR-RAM game PCB (NES 2.0 submapper 0).
 * DIP/solder-pad ROM read and CHR-RAM write protection for multicart-specific
 * submappers 1/2 are NOT emulated. No accessory or NOAC required.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
class AuroraVn42Mapper227 : public Nes_Mapper {
    uint16_t address;
    void sync() {
        const unsigned a=(unsigned)address;
        const unsigned prg=((a>>3)&0x20U)|((a>>2)&0x1fU);
        const unsigned a14=a&1U;
        const bool nrom=((a>>7)&1U)!=0;
        const bool unrom=((a>>9)&1U)!=0;
        const unsigned bank0=prg&~a14;
        const unsigned bank1=((prg|a14)&~(7U*(!nrom)*(!unrom)))
                                 |(7U*(!nrom)*(unrom));
        set_prg_bank(0x8000,bank_16k,(int)bank0);
        set_prg_bank(0xc000,bank_16k,(int)bank1);
        if(a&2U)mirror_vert();else mirror_horiz();
    }
public:
    AuroraVn42Mapper227():address(0) { register_state(&address,sizeof address); }
    virtual void reset_state() { address=0; }
    virtual void apply_mapping() {
        enable_sram(true);
        if(cart().chr_size()) set_chr_bank(0,bank_8k,0);
        else set_chr_ram_bank(0,bank_8k,0);
        sync();
    }
    virtual void write(nes_time_t,nes_addr_t a,int) {
        if(a<0x8000 || a>0xffff || address==(uint16_t)a)return;
        address=(uint16_t)a;sync();
    }
};
