#pragma once
/* SPDX-License-Identifier: GPL-2.0-or-later
 * Aurora vN39 Mapper 194: Waixing MMC3 derivative with 2 KiB CHR-RAM.
 * Source: FCEUmm-next mapper194.c, GPL-2.0-or-later.
 */
#include "mapper004.hpp"
class AuroraVn39Mapper194 : public Mapper004 {
    void map_chr1(nes_addr_t addr,unsigned bank){
        if(cart().chr_size()==0 || (bank&0xfeU)==0U)
            set_chr_ram_bank(addr,bank_1k,bank&1U);
        else set_chr_bank(addr,bank_1k,int(bank));
    }
public:
    virtual long aux_chr_ram_size() const {return 2048;}
    virtual void update_chr_banks(){
        const nes_addr_t x=(mode&0x80U)?0x1000:0;
        const unsigned b0=banks[0]&0xfeU,b1=banks[1]&0xfeU;
        map_chr1(0x0000^x,b0);map_chr1(0x0400^x,b0|1U);
        map_chr1(0x0800^x,b1);map_chr1(0x0c00^x,b1|1U);
        for(int i=0;i<4;i++)map_chr1((0x1000+i*0x400)^x,banks[2+i]);
    }
};
