#pragma once
/* Mapper 47, officially licensed Nintendo 2-in-1 multicart (Super Spike
 * V'Ball / Nintendo World Cup). MMC3 registers + outer bank latch.
 * FCEUmm-next mapper047.c, GPL-2.0-or-later.
 * MMC3 virtual clock/IRQ and bytewise state reused from Mapper004.
 */
#include "mapper004.hpp"
#include <stdint.h>
class AuroraVn35Mapper047 : public Mapper004 {
    uint8_t outer;
    void sync_prg() {
        const int bankbase=int(outer)<<4;
        const nes_addr_t sw=(mode&0x40)?0x4000:0;
        set_prg_bank(0x8000 ^ sw,bank_8k,bankbase|(banks[6]&15));
        set_prg_bank(0xa000,bank_8k,bankbase|(banks[7]&15));
        set_prg_bank(0xc000 ^ sw,bank_8k,bankbase|14);
        set_prg_bank(0xe000,bank_8k,bankbase|15);
    }
    void sync_chr() {
        const int base=int(outer)<<7;
        const nes_addr_t sw=(mode&0x80)?0x1000:0;
        set_chr_bank(0x0000 ^ sw,bank_2k, (base |(banks[0]&0x7e))>>1);
        set_chr_bank(0x0800 ^ sw,bank_2k, (base |(banks[1]&0x7e))>>1);
        for(int i=0;i<4;i++) {
            set_chr_bank((0x1000+(i*0x400)) ^ sw,
                         bank_1k, base|(banks[i+2]&0x7f));
        }
    }
public:
    AuroraVn35Mapper047():outer(0){}
    virtual void reset_state(){Mapper004::reset_state();outer=0;}
    virtual void apply_mapping(){
        Mapper004::write(0,0xa000,mirror);
        Mapper004::write(0,0xa001,sram_mode);
        sync_prg();sync_chr();start_frame();
        intercept_writes(0x6000,0x2000);
    }
    virtual bool write_intercepted(nes_time_t,nes_addr_t addr,int data){
        if(addr<0x6000||addr>=0x8000)return false;
        if((sram_mode & 0xc0)==0x80) {
            outer=(uint8_t)data;sync_prg();sync_chr();
        }
        return true; // no SRAM on mapper 47
    }
    virtual void write(nes_time_t time,nes_addr_t addr,int data){
        Mapper004::write(time,addr,data);
        if((addr&0xe001)==0x8000 || (addr&0xe001)==0x8001){
            sync_prg();sync_chr();
        }
    }
    // MMC3 registers occupy 15 bytes in the historical state format.
    // Append only one byte: preserves exact base-state prefix for compat.
    virtual void save_state(mapper_state_t&out){
        Mapper004::save_state(out);
        if(out.size >= 0 && out.size < (int)sizeof out.data){
            out.data[out.size++]=outer;
        }
    }
    virtual void read_state(mapper_state_t const&in){
        Mapper004::read_state(in);
        outer=(in.size>15)?in.data[15]:0;
    }
};
