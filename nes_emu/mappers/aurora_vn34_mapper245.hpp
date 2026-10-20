#pragma once
/* SPDX-License-Identifier: GPL-2.0-or-later
 * Mapper 245 - MMC3 relative bank selection (FCEUmm mapper245.c).
 * Reuse QuickNES MMC3 IRQ, timing, SRAM and snapshot state.
 * THIS subclass does not register a second state buffer; bank0 state
 * already lives in inherited mmc3_state_t. Deliberately override write
 * and apply_mapping, because Mapper004::update_prg_banks is NON-VIRTUAL.
 */
#include "mapper004.hpp"
class AuroraVn34Mapper245 : public Mapper004 {
    void sync_prg(){
        const int high=(banks[0]&2)<<5;
        const int sw=(mode&0x40)?0x4000:0;
        set_prg_bank(0x8000^sw,bank_8k,high|(banks[6]&0x3f));
        set_prg_bank(0xa000,bank_8k,high|(banks[7]&0x3f));
        set_prg_bank(0xc000^sw,bank_8k,high|0x3e);
        set_prg_bank(0xe000,bank_8k,high|0x3f);
    }
    void sync_chr(){
        // No CHR bank switching on this PCB; CHR RAM if the image has no CHR ROM.
        if(cart().chr_size()>0)set_chr_bank(0,bank_8k,0);
        else set_chr_ram_bank(0,bank_8k,0);
    }
public:
    virtual void apply_mapping(){
        Mapper004::write(0,0xa000,mirror);
        Mapper004::write(0,0xa001,sram_mode);
        sync_prg();sync_chr();start_frame();
    }
    virtual void write(nes_time_t time,nes_addr_t a,int d){
        switch(a&0xe001){
        case 0x8000:
            mode=uint8_t(d);sync_prg();sync_chr();return;
        case 0x8001:
            banks[mode&7]=uint8_t(d);sync_prg();sync_chr();return;
        default:
            Mapper004::write(time,a,d);return;
        }
    }
};
