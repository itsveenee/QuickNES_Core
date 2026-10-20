/* SPDX-License-Identifier: GPL-2.0-or-later
 * FCEUmm-next board reference / Aurora mapper adaptation.
 */
#pragma once
/* Aurora QuickNES vN33 / Mapper 148 SA-008-A / Tengen 800008.
 * Discrete ROM bus conflicts: effective data = CPU data AND old ROM byte.
 * High-ROM writes only, mapper-local; no global bus behavior change.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
class AuroraVn33Mapper148 : public Nes_Mapper {
    uint8_t latch;
public:
    AuroraVn33Mapper148(){register_state(&latch,1);}
    virtual void reset_state(){latch=0;}
    virtual void apply_mapping(){set_prg_bank(0x8000,bank_32k,latch>>3);set_chr_bank(0,bank_8k,latch&7);}
    virtual void write(nes_time_t,nes_addr_t a,int v){
        const long size=cart().prg_size();
        if(size<=0||!cart().prg())return;
        const unsigned long addr=(unsigned long)((latch>>3)*0x8000u+(a&0x7fff));
        latch=uint8_t(v&cart().prg()[addr%(unsigned long)size]);
        apply_mapping();
    }
};
