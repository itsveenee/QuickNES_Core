#pragma once
/* Mapper 238, Contra Fighter board: MMC3 + 2-bit protection register.
 * FCEUmm-next mapper238.c. Keep original MMC3 IRQ and banking intact.
 */
#include "mapper004.hpp"
#include <stdint.h>
class AuroraVn35Mapper238 : public Mapper004 {
    uint8_t protection;
public:
    AuroraVn35Mapper238():protection(0){}
    virtual void reset_state(){Mapper004::reset_state();protection=0;}
    virtual void apply_mapping(){
        Mapper004::apply_mapping();
        intercept_reads(0x4000,0x2000);
        intercept_writes(0x4000,0x2000);
    }
    virtual bool write_intercepted(nes_time_t,nes_addr_t a,int data){
        if(a<0x4020 || a>=0x6000)return false;
        static const uint8_t lut[4]={0,2,2,3};
        protection=lut[data&3];return true;
    }
    virtual int read(nes_time_t,nes_addr_t a){
        if(a>=0x4020 && a<0x6000)return protection;
        return -1;
    }
    virtual void save_state(mapper_state_t&out){
        Mapper004::save_state(out);
        if(out.size >= 0 && out.size < (int)sizeof out.data){
            out.data[out.size++]=protection;
        }
    }
    virtual void read_state(mapper_state_t const&in){
        Mapper004::read_state(in);
        protection=(in.size>15)?in.data[15]:0;
    }
};
