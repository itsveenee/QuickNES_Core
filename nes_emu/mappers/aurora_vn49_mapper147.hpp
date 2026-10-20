#pragma once
/* Aurora QuickNES vN49 - TC-U01/JV001, mapper 147.
 * Source: FCEUmm-next hw/jv001.c and mapper147.c.
 * No IRQ, external devices, DIP or shared-core modifications. */
#include "Nes_Mapper.h"
#include <stdint.h>
#include <string.h>
class AuroraVn49Mapper147 : public Nes_Mapper {
    enum { ACC=0, INV=1, STAGE=2, OUT=3, INC=4, X=5, FLIP=6 };
    uint8_t r[7];
    static uint8_t rot_in(uint8_t d){return uint8_t((d>>2)|(d<<6));}
    static uint8_t rot_out(uint8_t d){return uint8_t((d<<2)|(d>>6));}
    void sync(){
        set_prg_bank(0x8000,bank_32k,((r[OUT]>>4)&2U)|(r[OUT]&1U));
        set_chr_bank(0,bank_8k,(r[OUT]>>1)&0x0FU);
    }
    void low_write(nes_addr_t a,uint8_t d){
        switch(a&0x103U){
        case 0x100U:
            if(r[INC])++r[ACC];
            else r[ACC]=uint8_t((r[ACC]&0xF0U)|((r[STAGE]^r[FLIP])&0x0FU));
            break;
        case 0x101U:r[FLIP]=(d&1U)?0xFFU:0;break;
        case 0x102U:r[STAGE]=d&0x0FU;r[INV]=d&0xF0U;break;
        case 0x103U:r[INC]=d&1U;break;
        }
        r[X]=r[FLIP]?0U:1U;
        sync();
    }
public:
    AuroraVn49Mapper147(){register_state(r,sizeof r);}
    virtual void reset_state(){memset(r,0,sizeof r);r[FLIP]=0xFFU;}
    virtual void apply_mapping(){
        intercept_reads(0x4100,0x1F00);
        intercept_writes(0x4100,0x1F00);
        sync();
    }
    virtual bool write_intercepted(nes_time_t,nes_addr_t a,int data){
        if(a<0x4100U||a>0x5FFFU)return false;
        low_write(a,rot_in(uint8_t(data)));
        return true;
    }
    virtual int read(nes_time_t,nes_addr_t a){
        if(a<0x4100U||a>0x5FFFU)return -1;
        uint8_t value=uint8_t(a>>8); // open-bus approximation, as in QuickNES
        if((a&0x103U)==0x100U){
            value=uint8_t((r[ACC]&0x0FU)|((r[INV]^r[FLIP])&0xF0U));
            sync();
        }
        return rot_out(value);
    }
    virtual void write(nes_time_t,nes_addr_t a,int data){
        if(a<0x8000U)return;
        r[OUT]=uint8_t((r[ACC]&0x0FU)|(r[INV]&0xF0U));
        r[X]=r[FLIP]?0U:1U;
        (void)data;
        sync();
    }
};
