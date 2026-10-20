#pragma once
/* SPDX-License-Identifier: GPL-2.0-or-later
 * Aurora vN45 mapper 228, Action 52/Active Enterprises.
 * 16K/32K PRG, address + data latches, 8K CHR, and fixed H/V mirror.
 * The original 1.5 MiB Action 52 ROM has a 512KiB disconnected window:
 * logical $100000-$17FFFF behaves as byte (address>>8)&255, while the
 * third physical 512KiB chip is logically at $180000-$1FFFFF.
 * We map a reusable 64KiB synthetic ROM pattern, NOT a 2MiB ROM copy.
 * No disputed 4-nibble DIP/RAM at $4020-$4023 (neither retail cartridge
 * needs or writes it); no peripheral support.
 * Reference: FCEUmm-next src/mappers/mapper228.c.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
class AuroraVn45Mapper228 : public Nes_Mapper {
    uint8_t regs[3]; // CPU latched address little-endian, latched data.
    uint8_t hole_pattern[65536];
    unsigned addr() const {return unsigned(regs[0]) | (unsigned(regs[1])<<8);}
    void prg16(nes_addr_t cpu,unsigned logical){
        if(cart().prg_size()!=0x180000L){
            set_prg_bank(cpu,bank_16k,int(logical));
            return;
        }
        if(logical>=64 && logical<96){
            const unsigned physical_offset=(logical-64U)*16384U;
            for(unsigned j=0;j<16384U;j+=Nes_Cpu::page_size)
                map_cpu_rom_page(cpu+j,hole_pattern+((physical_offset+j)&65535U));
        }else{
            /* last real ROM chip is physically $100000-$17FFFF */
            unsigned physical_bank=logical>=96?logical-32:logical;
            set_prg_bank(cpu,bank_16k,int(physical_bank));
        }
    }
    void sync(){
        const unsigned a=addr();
        if(a&0x20){
            const unsigned logical=(a>>6)&0x7F;
            prg16(0x8000,logical);
            prg16(0xC000,logical);
        }else{
            const unsigned bank=(a>>7)&0x3F;
            prg16(0x8000,bank*2);
            prg16(0xC000,bank*2+1);
        }
        set_chr_bank(0,bank_8k,int(((a<<2)&0x3C)|(regs[2]&0x03)));
        if(a&0x2000)mirror_horiz();else mirror_vert();
    }
public:
    AuroraVn45Mapper228(){
        register_state(regs,sizeof regs);
        for(unsigned i=0;i<sizeof hole_pattern;++i)
            hole_pattern[i]=uint8_t((i>>8)&255U);
    }
    virtual void reset_state(){regs[0]=regs[1]=regs[2]=0;}
    virtual void apply_mapping(){sync();}
    virtual void write(nes_time_t,nes_addr_t address,int data){
        regs[0]=uint8_t(address); regs[1]=uint8_t(address>>8);
        regs[2]=uint8_t(data);sync();
    }
};
