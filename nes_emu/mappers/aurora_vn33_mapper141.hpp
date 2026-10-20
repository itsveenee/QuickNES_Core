/* SPDX-License-Identifier: GPL-2.0-or-later
 * FCEUmm-next board reference / Aurora mapper adaptation.
 */
#pragma once
/* Aurora QuickNES vN33 - Sachen 8259 variants B/C/A: 138, 139, 141.
 * Reconstructed from FCEUmm-next mapper138/139/141.c.
 * Mapper-specific state; no core changes. C++98.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
#include <string.h>
#ifndef AURORA_VN33_8259_COMMON
#define AURORA_VN33_8259_COMMON
template<int MODEL> class AuroraVn33Sachen8259 : public Nes_Mapper {
    struct state_t { uint8_t cmd; uint8_t reg[8]; } r;
    void sync_prg(){set_prg_bank(0x8000,bank_32k,r.reg[5]);}
    void sync_chr(){
        if(cart().chr_size()<=0)return;
        const unsigned base=unsigned(r.reg[4])<<3;
        for(int i=0;i<4;++i){
            const unsigned input=r.reg[(r.reg[7]&1)?0:i]&7;
            unsigned idx=base|input;
            if(MODEL==139)idx=(idx<<2)|unsigned(i);
            else if(MODEL==141)idx=(idx<<1)|unsigned(i&1);
            set_chr_bank(0x800*i,bank_2k,int(idx));
        }
    }
    void sync_mirror(){
        switch(r.reg[7]&7){
            case 0:mirror_manual(0,0,0,1);break;
            case 2:mirror_horiz();break;
            case 6:mirror_single(0);break;
            default:mirror_vert();break;
        }
    }
    void write_reg(nes_addr_t a,int data){
        if(!(a&0x4000)||!(a&0x100))return;
        if(!(a&1)){r.cmd=uint8_t(data);return;}
        unsigned index=r.cmd&7;
        r.reg[index]=uint8_t(data);
        if(index==5)sync_prg();
        else if(index==7){sync_chr();sync_mirror();}
        else if(index!=0xff)sync_chr();
    }
public:
    AuroraVn33Sachen8259(){register_state(&r,sizeof r);}
    virtual void reset_state(){memset(&r,0,sizeof r);}
    virtual void apply_mapping(){sync_prg();sync_chr();sync_mirror();intercept_writes(0x4000,0x4000);}
    virtual bool write_intercepted(nes_time_t,nes_addr_t a,int d){
        if(a<0x4000||a>=0x8000||!(a&0x4000)||!(a&0x100))return false;
        write_reg(a,d);return true;
    }
    virtual void write(nes_time_t,nes_addr_t a,int d){write_reg(a,d);}
};
#endif

class AuroraVn33Mapper141 : public AuroraVn33Sachen8259<141> {};
