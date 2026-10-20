/* SPDX-License-Identifier: GPL-2.0-or-later
 * FCEUmm-next board reference / Aurora mapper adaptation.
 */
#pragma once
/* Aurora QuickNES vN33 / Mapper 144 AGCI 50282 (Death Race).
 * PRG ROM D0 wins the bus and is OR-ed with CPU D0, BEFORE mapping changes.
 * Verified against NESdev Death Race board discussion and staging.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
class AuroraVn33Mapper144 : public Nes_Mapper {
    uint8_t latch;
public:
    AuroraVn33Mapper144(){register_state(&latch,1);}
    virtual void reset_state(){latch=0;}
    virtual void apply_mapping(){set_prg_bank(0x8000,bank_32k,latch&15);set_chr_bank(0,bank_8k,latch>>4);}
    virtual void write(nes_time_t,nes_addr_t a,int v){
        const long size=cart().prg_size();
        if(size<=0||!cart().prg())return;
        // Physical ROM at the *old* bank, not bank selected by this write.
        const unsigned long addr=(unsigned long)((latch&15)*0x8000u+(a&0x7fff));
        const uint8_t oldrom=cart().prg()[addr%(unsigned long)size];
        latch=uint8_t(v|(oldrom&1));
        apply_mapping();
    }
};
