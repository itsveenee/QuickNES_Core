#pragma once
/* SPDX-License-Identifier: GPL-2.0-or-later
 * Mapper 224 / Waixing Type J; inherits Type I's onboard SRAM windows.
 * Four independent PRG windows, MMC3 CHR/mirroring/IRQ. No DIP.
 * Sources: Nestopia NstBoardWaixing.cpp, NstBoardWaixing.hpp.
 */
#include "aurora_vn48_mapper223.hpp"
#include <stdint.h>
#include <string.h>
class AuroraVn48Mapper224 : public AuroraVn48Mapper223 {
    uint8_t prg_extra[4];
    enum { mmc_state_bytes=sizeof(mmc3_state_t) };
    void sync_prg(){
        const unsigned inversion=(mode>>6)&1U;
        set_prg_bank(0x8000,bank_8k,prg_extra[inversion?2:0]);
        set_prg_bank(0xA000,bank_8k,prg_extra[1]);
        set_prg_bank(0xC000,bank_8k,prg_extra[inversion?0:2]);
        set_prg_bank(0xE000,bank_8k,prg_extra[3]);
    }
public:
    AuroraVn48Mapper224(){memset(prg_extra,0,sizeof(prg_extra));}
    virtual void reset_state(){
        Mapper004::reset_state();
        prg_extra[0]=1;prg_extra[1]=2;
        prg_extra[2]=0x7e;prg_extra[3]=0x7f;
    }
    virtual void apply_mapping(){
        AuroraVn48Mapper223::apply_mapping();
        sync_prg();
    }
    virtual void write(nes_time_t t,nes_addr_t addr,int data){
        const unsigned sel=addr & 0xe001;
        if(sel==0x8001 && (mode&7)>=6)
            prg_extra[(mode&7)-6]=(uint8_t)data;
        Mapper004::write(t,addr,data);
        if(sel==0x8000 || sel==0x8001)sync_prg();
    }
    virtual void save_state(mapper_state_t&out){
        Mapper004::save_state(out);
        if(out.size==mmc_state_bytes &&
            out.size+(int)sizeof(prg_extra)<=(int)max_mapper_state_size){
            memcpy(out.data+out.size,prg_extra,sizeof(prg_extra));
            out.size+=(int)sizeof(prg_extra);
        }
    }
    virtual void read_state(mapper_state_t const&in){
        if(in.size>=(int)(mmc_state_bytes+sizeof(prg_extra)))
            memcpy(prg_extra,in.data+mmc_state_bytes,sizeof(prg_extra));
        else {
            prg_extra[0]=1;prg_extra[1]=2;
            prg_extra[2]=0x7e;prg_extra[3]=0x7f;
        }
        Mapper004::read_state(in);
        sync_prg();
    }
};
