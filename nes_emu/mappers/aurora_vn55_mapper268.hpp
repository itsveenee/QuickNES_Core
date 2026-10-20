#pragma once
/* SNESticleAurora QuickNES vN55 — NES2 mapper 268, submappers 0/1 ONLY.
 * Reference: negativeExponent/libretro-fceumm_next mapper268.c, hw/mmc3.c.
 * COOLBOY / MINDKIDS: on-cartridge ASIC, no DIP, no keyboard/peripheral.
 * This deliberately excludes later PCB revisions (submappers >=2) and
 * unverified special MMC4-like modes; ROM compatibility NOT established.
 * Reuses native MMC3 IRQ scheduler. Does not alter shared CPU/PPU logic.
 */
#include "mapper004.hpp"
#include <stdint.h>
#include <string.h>

class AuroraVn55Mapper268 : public Mapper004 {
    uint8_t ex[8];
    unsigned outer_prg_base() const {
        return (unsigned(ex[3]) & 0x0eU) |
            ((unsigned(ex[0]) << 4) & 0x070U) |
            ((unsigned(ex[1]) << 3) & 0x080U) |
            ((unsigned(ex[1]) << 6) & 0x300U) |
            ((unsigned(ex[0]) << 6) & 0xc00U) |
            ((unsigned(~ex[1]) << 12) & 0x1000U);
    }
    unsigned prg_mask(bool gnrom) const {
        return (gnrom ? 0U : 0x0fU) |
            ((unsigned(~ex[0]) >> 2) & 0x10U) |
            ((unsigned(~ex[1]) >> 2) & 0x20U) |
            ((unsigned(ex[1]) >> 0) & 0x40U) |
            ((unsigned(ex[1]) << 2) & 0x80U);
    }
    void map_prg(nes_addr_t addr, unsigned bank) {
        const bool gnrom = (ex[3] & 0x10U) != 0;
        const unsigned mg = gnrom ? ((ex[1] & 2U) ? 3U : 1U) : 0U;
        const unsigned mm = prg_mask(gnrom), base = outer_prg_base();
        const unsigned mask = mm | mg;
        const unsigned result = (base & ~mask) | (bank & mm) | ((addr >> 13) & mg);
        set_prg_bank(addr, bank_8k, int(result));
    }
    unsigned bank_chr(unsigned addr,unsigned bank) const {
        const unsigned base = ((unsigned(ex[0]) << 4) & 0x380U) |
                              ((unsigned(ex[2]) << 3) & 0x078U);
        const bool gnrom = (ex[3] & 0x10U) != 0;
        const unsigned mm = gnrom ? 0U : ((ex[0] & 0x80U) ? 0x7fU : 0xffU);
        const unsigned mg = gnrom ? 7U : 0U;
        const unsigned mask=mm|mg;
        return (base & ~mask) | (bank & mm) | ((addr >> 10) & mg);
    }
    void update_prg_banks() {
        unsigned b[4] = {banks[6],banks[7],0xfeU,0xffU};
        if (mode & 0x40U) {unsigned tmp=b[0];b[0]=b[2];b[2]=tmp;}
        for(unsigned i=0;i<4U;++i)map_prg(0x8000U+(i*0x2000U),b[i]);
        /* FCEUmm documents the mode below as incomplete even upstream.
         * Implement its present fixed-bank rule, not hypothetical latches. */
        if (ex[3] & 0x40U) {
            if (!(mode & 0x40U)) {
                const bool gnrom=(ex[3]&0x10U)!=0;
                const unsigned mg=gnrom?((ex[1]&2U)?3U:1U):0U;
                const unsigned mm=prg_mask(gnrom),base=outer_prg_base();
                const unsigned mask=mm|mg;
                set_prg_bank(0xc000,bank_8k,int((base&~mask)|(2U&mg)));
                set_prg_bank(0xe000,bank_8k,int((base&~mask)|(3U&mg)));
            }
        }
    }
    void update_chr_banks() {
        const unsigned values[8] = {
            unsigned(banks[0])&0xfeU, unsigned(banks[0])|1U,
            unsigned(banks[1])&0xfeU, unsigned(banks[1])|1U,
            banks[2],banks[3],banks[4],banks[5]
        };
        for(unsigned i=0;i<8U;++i) {
            const unsigned a=(i ^ ((mode&0x80U)?4U:0U))*0x400U;
            const unsigned b=bank_chr(a,values[i]);
            if (cart().chr_size()>0 && (ex[4] & 1U) &&
                  ((values[i]&0xfeU)==(unsigned(ex[4])&0xfeU)))
                set_chr_ram_bank(a,bank_1k,int(b));
            else set_chr_bank(a,bank_1k,int(b));
        }
        /* Upstream's incomplete MMC4-like mode remaps first four pages. */
        if (ex[3] & 0x40U) {
            unsigned swap=(mode&0x80U)?0x1000U:0U;
            for(unsigned i=0;i<4U;++i) {
                unsigned v=(i&1U)?0U:unsigned(banks[i/2U]);
                set_chr_bank((i*0x400U)^swap,bank_1k,int(bank_chr(i*0x400U,v)));
            }
        }
    }
    void write_extra(nes_addr_t addr,unsigned value) {
        const unsigned i=addr&7U;
        if ((ex[3]&0x80U) && i!=2U) return;
        if (i==2U) {
            if (ex[2]&0x80U) value=(value&0x0fU)|(ex[2]&~0x0fU);
            value &= (0xf1U | ((unsigned(~ex[2]) >> 3)&0x0eU));
        }
        ex[i]=uint8_t(value);
        update_prg_banks();update_chr_banks();
    }
public:
    AuroraVn55Mapper268(){memset(ex,0,sizeof(ex));}
    virtual long aux_chr_ram_size() const {
        /* Mixed CHR-ROM/RAM cartridges use on-board 8 KiB CHR-RAM.
           Pure CHR-RAM cartridges use QuickNES' normal 8 KiB path. */
        return cart().chr_size()>0 ? 0x2000L : 0L;
    }
    virtual void reset_state() {
        Mapper004::reset_state();
        memset(ex,0,sizeof(ex));
        const bool sub1=cart().submapper_code()==1;
        intercept_writes(sub1?0x5000:0x6000,sub1?0x1000:0x2000);
    }
    virtual void apply_mapping() {
        Mapper004::apply_mapping();
        if (!cart().submapper_code()) intercept_reads(0x6000,0x2000);
    }
    virtual int read(nes_time_t,nes_addr_t addr) {
        if (!cart().submapper_code() && addr>=0x6000U && addr<0x8000U) {
            if (sram_mode&0xa0U) return sram_read_byte(addr);
            return int(addr>>8); // existing QuickNES approximation of open bus
        }
        return -1;
    }
    virtual bool write_intercepted(nes_time_t,nes_addr_t addr,int value) {
        const bool sub1=cart().submapper_code()==1;
        if ((sub1 && (addr<0x5000U||addr>=0x6000U)) ||
            (!sub1 && (addr<0x6000U||addr>=0x8000U))) return false;
        if (!sub1 && ((sram_mode&0xc0U)==0x80U || (sram_mode&0x20U)))
            sram_write_byte(addr,uint8_t(value));
        write_extra(addr,unsigned(value)&0xffU);
        return true;
    }
    virtual void save_state(mapper_state_t &out) {
        uint8_t bytes[23];
        memcpy(bytes,static_cast<mmc3_state_t*>(this),15);
        memcpy(bytes+15,ex,8);
        out.write(bytes,23);
    }
    virtual void read_state(mapper_state_t const&in) {
        uint8_t bytes[23]={0};
        in.read(bytes,23);
        memcpy(static_cast<mmc3_state_t*>(this),bytes,15);
        memcpy(ex,bytes+15,8);
        apply_mapping();
    }
};
