#pragma once
/* SPDX-License-Identifier: GPL-2.0-or-later
 * Aurora vN40 Mapper 243: Sachen SA-020A, Honey Peach.
 * Eight readable register values, PRG32, CHR8, four mirroring modes.
 * NESdev iNES Mapper 243 / register details.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
#include <string.h>
class AuroraVn40Mapper243 : public Nes_Mapper {
    uint8_t regs[8];
    uint8_t index;
    void sync_banks(){
        const unsigned chr=(regs[2]&1U)|((regs[4]&1U)<<1)|((regs[6]&3U)<<2);
        set_prg_bank(0x8000,bank_32k,regs[5]&3U);
        set_chr_bank(0,bank_8k,chr);
    }
    void sync_mirror(){
        switch((regs[7]>>1)&3U){
        case 0:mirror_manual(0,0,0,1);break;
        case 1:mirror_horiz();break;
        case 2:mirror_vert();break;
        case 3:mirror_single(1);break;
        }
    }
public:
    AuroraVn40Mapper243():index(0){register_state(regs,sizeof regs);}
    virtual void reset_state(){memset(regs,0,sizeof regs);index=0;}
    virtual void apply_mapping(){
        sync_banks();sync_mirror();
        intercept_writes(0x4000,0x2000);
        intercept_reads(0x4000,0x2000);
    }
    virtual bool write_intercepted(nes_time_t,nes_addr_t a,int d){
        if((a&0xc101)!=0x4100 && (a&0xc101)!=0x4101)return false;
        if((a&1)==0)index=(uint8_t)d&7U;
        else{
            regs[index]=(uint8_t)d;
            if(index==2 || index==4 || index==5 || index==6)sync_banks();
            else if(index==7)sync_mirror();
        }
        return true;
    }
    virtual int read(nes_time_t,nes_addr_t a){
        if((a&0xc101)==0x4100)return index;
        if((a&0xc101)==0x4101)return regs[index];
        return -1;
    }
    virtual void write(nes_time_t,nes_addr_t,int){}
    virtual void save_state(mapper_state_t&out){
        Nes_Mapper::save_state(out);
        if(out.size>=0 && out.size<(int)sizeof(out.data))out.data[out.size++]=index;
    }
    virtual void read_state(mapper_state_t const&in){
        Nes_Mapper::read_state(in);
        index=(in.size>8)?(in.data[8]&7U):0;
    }
};
