#pragma once
/* SNESticleAurora QuickNES vN50 - NES mapper 132 / TXC and CRC-exact
 * legacy UNL-22211 PCB; software-only mode selection, no DIP support.
 * Based on FCEUmm-next src/mappers/mapper132.c,
 * src/mappers/mapper132_unl22211.c, src/mappers/hw/txc.c.
 * FCEUmm's ROM CRC is CRC32(PRG||CHR), excluding iNES header.
 * QuickNES does not retain full CPU open bus; upper undriven bits are
 * approximated using the same address-high fallback as mappers 36/173.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
#include <string.h>
class AuroraVn50Mapper132 : public Nes_Mapper {
    enum { ACC=0, INV=1, STAGE=2, OUT=3, INC=4, X=5, Y=6, FLIP=7 };
    uint8_t regs[12]; // TXC 8 bytes + UNL-22211 4 bytes; all serialized
    bool legacy;
    bool checked;
    static uint32_t crc32_update(uint32_t crc,const uint8_t*bytes,long count){
        while(count-->0){
            crc^=(uint32_t)(*bytes++);
            for(int i=0;i<8;++i)crc=(crc>>1)^((crc&1U)?0xEDB88320UL:0U);
        }
        return crc;
    }
    bool is_legacy_rom()const{
        if(cart().prg_size()<0 || cart().chr_size()<0)return false;
        if(cart().prg_size()>0 && !cart().prg())return false;
        if(cart().chr_size()>0 && !cart().chr())return false;
        uint32_t crc=0xFFFFFFFFUL;
        crc=crc32_update(crc,cart().prg(),cart().prg_size());
        crc=crc32_update(crc,cart().chr(),cart().chr_size());
        return (crc^0xFFFFFFFFUL)==0x2A5F4C5AUL;
    }
    void sync(){
        unsigned prg=legacy?((regs[10]>>2)&1U):((regs[OUT]>>2)&1U);
        unsigned chr=legacy?(regs[10]&3U):(regs[OUT]&3U);
        set_prg_bank(0x8000,bank_32k,(int)prg);
        set_chr_bank(0,bank_8k,(int)chr);
    }
    void txc_flags(uint8_t data){
        regs[X]=regs[FLIP]?0U:1U;
        regs[Y]=(uint8_t)(regs[X] || (data&0x10U));
    }
    void txc_write(nes_addr_t addr,uint8_t data){
        if(addr&0x8000U){
            regs[OUT]=(uint8_t)((regs[ACC]&15U)|((regs[INV]<<1)&0x10U));
        }else{
            switch(addr&0x103U){
            case 0x100U:
                if(regs[INC]) ++regs[ACC];
                else regs[ACC]=(uint8_t)((regs[ACC]&0xF8U)|((regs[STAGE]^regs[FLIP])&7U));
                break;
            case 0x101U:regs[FLIP]=(data&1U)?255U:0U;break;
            case 0x102U:regs[STAGE]=(uint8_t)(data&7U);regs[INV]=(uint8_t)(data&0xF8U);break;
            case 0x103U:regs[INC]=(uint8_t)(data&1U);break;
            }
        }
        txc_flags(data);
        sync();
    }
    uint8_t txc_read(nes_addr_t addr){
        if((addr&0x103U)!=0x100U)return (uint8_t)(addr>>8);
        uint8_t v=(uint8_t)((regs[ACC]&7U)|((regs[INV]^regs[FLIP])&0xF8U));
        regs[Y]=(uint8_t)(regs[X] || (v&0x10U));
        sync();
        return v;
    }
public:
    AuroraVn50Mapper132():legacy(false),checked(false){
        register_state(regs,sizeof regs);
    }
    virtual void reset_state(){
        memset(regs,0,sizeof regs);
        legacy=false;checked=false;
    }
    virtual void apply_mapping(){
        if(!checked){legacy=is_legacy_rom();checked=true;}
        intercept_reads(0x4100,0x1F00);
        intercept_writes(0x4100,0x1F00);
        sync();
    }
    virtual bool write_intercepted(nes_time_t,nes_addr_t addr,int data){
        if(addr<0x4100U || addr>0x5FFFU)return false;
        if(legacy){
            if(addr<0x5000U && (addr&0x100U)){
                regs[8+(addr&3U)]=(uint8_t)data;
                sync();
            }
        }else{
            txc_write(addr,(uint8_t)(data&15));
        }
        return true;
    }
    virtual int read(nes_time_t,nes_addr_t addr){
        if(addr<0x4100U || addr>0x5FFFU)return -1;
        if(legacy){
            if(addr==0x4100U)return (int)((regs[9]^regs[10])|0x40U);
            return -1;
        }
        return (int)(((addr>>8)&0xF0U)|(txc_read(addr)&15U));
    }
    virtual void write(nes_time_t,nes_addr_t addr,int data){
        if(addr>=0x8000U && !legacy)txc_write(addr,(uint8_t)(data&15));
    }
};
