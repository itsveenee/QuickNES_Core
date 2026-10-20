#pragma once
/* Aurora QuickNES vN49 - TXC register unit extracted from FCEUmm-next
 * hw/txc.c; no modification of any other mapper or global CPU/PPU code.
 * The QuickNES CPU has no tracked full open bus; bits without hardware drivers
 * use QuickNES' existing high-address fallback approximation. */
#include "Nes_Mapper.h"
#include <stdint.h>
#include <string.h>
class AuroraVn49Mapper173 : public Nes_Mapper {
protected:
    enum { ACC=0, INV=1, STAGE=2, OUT=3, INC=4, X=5, Y=6, FLIP=7, CHR=8 };
    uint8_t r[8];
    void sync() {
        set_prg_bank(0x8000,bank_32k,0);
        if(cart().chr_size()>=16*1024)
            set_chr_bank(0,bank_8k,(r[OUT]&1U)|(r[Y]?2U:0U)|((r[OUT]&2U)<<1));
        else set_chr_bank(0,bank_8k,0);
    }
    void low_write(nes_addr_t a, uint8_t d) {
        switch (a & 0x103U) {
        case 0x100U:
            if (r[INC]) ++r[ACC];
            else r[ACC] = uint8_t((r[ACC] & 0xF8U) | ((r[STAGE]^r[FLIP])&7U));
            break;
        case 0x101U:r[FLIP] = (d&1U)?0xFFU:0; break;
        case 0x102U:r[STAGE]=d&7U;r[INV]=d&0xF8U;break;
        case 0x103U:r[INC]=d&1U;break;
        }
        r[X] = r[FLIP]?0U:1U;
        r[Y] = uint8_t(r[X] || (d&0x10U));
        sync();
    }
    uint8_t low_read(nes_addr_t a) {
        if ((a&0x103U)!=0x100U) return uint8_t(a>>8); // QuickNES open-bus fallback
        const uint8_t v=uint8_t((r[ACC]&7U)|((r[INV]^r[FLIP])&0xF8U));
        r[Y]=uint8_t(r[X] || (v&0x10U));
        sync();
        return v;
    }
public:
    AuroraVn49Mapper173(){register_state(r,sizeof r);}
    virtual void reset_state(){memset(r,0,sizeof r);}
    virtual void apply_mapping(){
        intercept_reads(0x4100,0x1F00);
        intercept_writes(0x4100,0x1F00);
        sync();
    }
    virtual bool write_intercepted(nes_time_t,nes_addr_t a,int data){
        if (a<0x4100U || a>0x5FFFU) return false;
        low_write(a,uint8_t(data&0x0F));
        return true;
    }
    virtual int read(nes_time_t,nes_addr_t a){
        if(a<0x4100U || a>0x5FFFU)return -1;
        const uint8_t v=low_read(a);
        return int(((a>>8)&0xF0U)|(v&0x0FU));
    }
    virtual void write(nes_time_t,nes_addr_t a,int data){
        if(a<0x8000U)return;
        r[OUT]=uint8_t((r[ACC]&0x0FU)|((r[INV]<<1)&0x10U));
        r[X]=r[FLIP]?0U:1U;
        r[Y]=r[X]; // high-write TXC input is data & 0x0f
        (void)data;
        sync();
    }
};
