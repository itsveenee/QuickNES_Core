#pragma once
/* SPDX-License-Identifier: GPL-2.0-or-later
 * Aurora vN39 Mapper 196: MRCM MMC3-clone; rewired A0 and optional
 * MMC3 x BNROM mode (FCEUmm-next mapper196.c, GPL-2.0-or-later).
 * Deliberately keep base MMC3 scanline IRQ unchanged.
 */
#include "mapper004.hpp"
#include <stdint.h>
class AuroraVn39Mapper196 : public Mapper004 {
    uint8_t ex[2]; // [0] NROM enabled, [1] latch
    void override_prg(){
        if(ex[0])set_prg_bank(0x8000,bank_32k,ex[1]>>4);
    }
    unsigned rewired_a0(nes_addr_t addr) const {
        switch(cart().submapper_code()){
        case 1:case 3:return (unsigned(addr)>>1)&1U;
        case 2:return (unsigned(addr)>>2)&1U;
        default:return (!!(addr&0x0e)) ^ (addr&1U);
        }
    }
public:
    AuroraVn39Mapper196(){ex[0]=ex[1]=0;}
    virtual void reset_state(){Mapper004::reset_state();ex[0]=ex[1]=0;}
    virtual void apply_mapping(){
        // Do not call Mapper004::apply_mapping() here: it invokes virtual
        // write(), which on MRCM rewires A0 and would misapply $A001 SRAM.
        // Call parent hardware operations without address rewriting.
        Mapper004::write(0,0xa000,mirror);
        Mapper004::write(0,0xa001,sram_mode);
        update_chr_banks();update_prg_banks();start_frame();
        override_prg();
        if(cart().submapper_code()==0 || cart().submapper_code()==3)
            intercept_writes(0x6000,0x2000);
    }
    virtual bool write_intercepted(nes_time_t,nes_addr_t addr,int data){
        if(addr<0x6000 || addr>=0x8000)return false;
        if(cart().submapper_code()!=0 && cart().submapper_code()!=3)return false;
        ex[0]=1;ex[1]=(uint8_t)data;override_prg();return true;
    }
    virtual void write(nes_time_t time,nes_addr_t addr,int data){
        const nes_addr_t decoded=(addr&0xf000)|nes_addr_t(rewired_a0(addr));
        Mapper004::write(time,decoded,data);
        // Base MMC3's PRG updater is nonvirtual: the 32 KiB override
        // must always be re-applied after bank-select or bank-data writes.
        override_prg();
    }
    virtual void save_state(mapper_state_t&out){
        Mapper004::save_state(out);
        if(out.size>=0 && out.size+2<=(int)sizeof(out.data)){
            out.data[out.size++]=ex[0];out.data[out.size++]=ex[1];
        }
    }
    virtual void read_state(mapper_state_t const&in){
        Mapper004::read_state(in);
        ex[0]=(in.size>15)?in.data[15]:0;
        ex[1]=(in.size>16)?in.data[16]:0;
    }
};
