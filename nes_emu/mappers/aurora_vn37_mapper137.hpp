#pragma once
/* QuickNES Aurora vN37 - Sachen 8259D (iNES 137).
 * FCEUmm-next src/mappers/mapper137.c and mapper137_alt.c (2026).
 * Board-specific A8/A14/A0 register decoding, CHR1K, PRG32, nametable mirroring.
 * Corrects source alternate's undefined m137_alt.reg[x] to reg[0].
 * Alternate identified by CRC32, checked once per reset/state restore.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
#include <string.h>
class AuroraVn37Mapper137 : public Nes_Mapper {
    uint8_t r[9]; // 0..7 bank/control, 8 command
    bool legacy;
    static uint32_t crc_step(uint32_t crc, uint8_t v) {
        crc ^= v;
        for(int i=0;i<8;i++)crc=(crc>>1)^((crc&1U)?0xedb88320UL:0UL);
        return crc;
    }
    static uint32_t crc_buffer(uint32_t crc, const uint8_t *p, long n) {
        if(!p || n<=0)return crc;
        for(long i=0;i<n;i++)crc=crc_step(crc,p[i]);
        return crc;
    }
    bool alternate_rom() const {
        // FCEUmm checks CartInfo.CRC32. Common ROM PRG+CHR checksum;
        // support the PRG-only alternative (legacy dbs) as well.
        uint32_t crc=crc_buffer(0xffffffffUL,cart().prg(),cart().prg_size());
        if((crc^0xffffffffUL)==0x3b4f48d0UL)return true;
        crc=crc_buffer(crc,cart().chr(),cart().chr_size());
        return (crc^0xffffffffUL)==0x3b4f48d0UL;
    }
    void sync_mirror(){
        if(legacy) {
            if(r[7]&1U){mirror_vert();return;}
            switch((r[7]>>1)&3U) {
            case 0:mirror_vert();break;
            case 1:mirror_horiz();break;
            case 2:mirror_manual(0,1,1,1);break;
            default:mirror_single(0);break;
            }
        } else {
            switch(r[7]&7U){
            case 2:mirror_vert();break;
            case 4:mirror_manual(0,0,0,1);break;
            case 6:mirror_single(0);break;
            default:mirror_horiz();break;
            }
        }
    }
    void sync(){
        set_prg_bank(0x8000,bank_32k,legacy?(r[5]&7):r[5]);
        const unsigned d0=r[0]&7U;
        const unsigned d1=(r[(r[7]&1U)?0:1]&7U)|((r[4]<<4)&16U);
        const unsigned d2=(r[(r[7]&1U)?0:2]&7U)|((r[4]<<3)&16U);
        const unsigned d3=(r[(r[7]&1U)?0:3]&7U)|((r[4]<<2)&16U)|((r[6]<<3)&8U);
        set_chr_bank(0x0000,bank_1k,d0);
        set_chr_bank(0x0400,bank_1k,d1);
        set_chr_bank(0x0800,bank_1k,d2);
        set_chr_bank(0x0c00,bank_1k,d3);
        set_chr_bank(0x1000,bank_4k,last_bank);
        sync_mirror();
    }
    void write_reg(nes_addr_t a,uint8_t v){
        if(legacy){
            // Alternate hardware maps writes at $4100-$7FFF, ignoring A14/A8
            // for data; command select is the exact $4100 folded address.
            if((a&0x4101)==0x4100){r[8]=v;return;}
            r[r[8]&7U]=v;sync();
        }else{
            if((a&0x4100)!=0x4100)return;
            if(a&1U){r[r[8]&7U]=v;sync();}
            else r[8]=v;
        }
    }
public:
    AuroraVn37Mapper137():legacy(false){register_state(r,sizeof r);}
    virtual void reset_state(){memset(r,0,sizeof r);legacy=alternate_rom();}
    virtual void apply_mapping(){legacy=alternate_rom();sync();intercept_writes(0x4000,0x4000);}
    virtual bool write_intercepted(nes_time_t,nes_addr_t a,int d){
        if(a<0x4100 || a>=0x8000)return false;
        if(legacy){write_reg(a,(uint8_t)d);return true;}
        if((a&0x4100)!=0x4100)return false;
        write_reg(a,(uint8_t)d);return true;
    }
    virtual void write(nes_time_t,nes_addr_t a,int d){
        if(a<0x8000)return;
        if(legacy)return;
        write_reg(a,(uint8_t)d);
    }
};
