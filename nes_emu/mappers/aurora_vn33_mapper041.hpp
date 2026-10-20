/* SPDX-License-Identifier: GPL-2.0-or-later
 * FCEUmm-next board reference / Aurora mapper adaptation.
 */
#pragma once
/* Aurora QuickNES vN33 / Mapper 41, explicitly retained Caltron 6-in-1.
 * NESdev: $6000-$67FF selects outer PRG/CHR and mirror using address lines;
 * $8000-$FFFF selects inner CHR 8K when outer PRG bank >=4, with AND bus conflict.
 * No PRG RAM; low register writes are fully intercepted.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
class AuroraVn33Mapper041 : public Nes_Mapper {
    uint8_t outer,inner;
    void sync(){
        set_prg_bank(0x8000,bank_32k,outer&7);
        set_chr_bank(0,bank_8k,((outer>>3)&3)*4+(inner&3));
        if(outer&0x20)mirror_horiz();else mirror_vert();
    }
public:
    AuroraVn33Mapper041(){register_state(&outer,2);}
    virtual void reset_state(){outer=0;inner=0;}
    virtual void apply_mapping(){sync();intercept_writes(0x6000,0x800);}
    virtual bool write_intercepted(nes_time_t,nes_addr_t a,int){
        if(a<0x6000||a>=0x6800)return false;
        outer=uint8_t(a&0x3f);sync();return true;
    }
    virtual void write(nes_time_t,nes_addr_t a,int v){
        if(!(outer&4))return; // Write permitted only in upper four PRG banks.
        const long size=cart().prg_size();
        if(size<=0||!cart().prg())return;
        const unsigned long addr=(unsigned long)((outer&7)*0x8000u+(a&0x7fff));
        inner=uint8_t((v&cart().prg()[addr%(unsigned long)size])&3);
        sync();
    }
};
