/* SPDX-License-Identifier: GPL-2.0-or-later
 * FCEUmm-next board reference / Aurora mapper adaptation.
 */
#pragma once
/* Aurora QuickNES vN33 / Mapper 149 Sachen SA-0036.
 * Fixed PRG32 bank 0; CHR8 D7; AND bus conflict per FCEUmm Latch_Init.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
class AuroraVn33Mapper149 : public Nes_Mapper {
    uint8_t latch;
public:
    AuroraVn33Mapper149(){register_state(&latch,1);}
    virtual void reset_state(){latch=0;}
    virtual void apply_mapping(){set_prg_bank(0x8000,bank_32k,0);set_chr_bank(0,bank_8k,latch>>7);}
    virtual void write(nes_time_t,nes_addr_t a,int v){
        const long size=cart().prg_size();
        if(size<=0||!cart().prg())return;
        latch=uint8_t(v&cart().prg()[(unsigned long)(a&0x7fff)%(unsigned long)size]);
        apply_mapping();
    }
};
