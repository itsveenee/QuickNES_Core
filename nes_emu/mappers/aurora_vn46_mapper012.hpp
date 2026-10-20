#pragma once
/* Aurora QuickNES vN46: mapper 12, Gouder SL-5020B (submapper 0 only).
 * IRQ: MMC3A (NEC) semantics: no IRQ on a zero-latch reload.
 * Only on-cartridge bank registers and game functionality are supported.
 * No FFE copier, physical DIP switches or other accessory emulation.
 * Primary references: NESdev INES Mapper 012, MMC3 chip revisions;
 * FCEUmm-next src/mappers/mapper012.c.
 * No changes to shared MMC3, CPU or PPU implementation.
 */
#include "mapper004.hpp"
#include <stdint.h>
#include <string.h>
class AuroraVn46Mapper012 : public Mapper004 {
    uint8_t chr_outer;
    void map_chr(){
        const unsigned x=(mode & 0x80U)?0x1000U:0U;
        const unsigned pages[8]={
            unsigned(banks[0]&0xfeU),unsigned((banks[0]&0xfeU)|1U),
            unsigned(banks[1]&0xfeU),unsigned((banks[1]&0xfeU)|1U),
            unsigned(banks[2]),unsigned(banks[3]),
            unsigned(banks[4]),unsigned(banks[5])};
        for(unsigned i=0;i<8;++i){
            const unsigned physical=(i*0x400U)^x;
            const unsigned bit=(physical&0x1000U)?0x10U:0x01U;
            const unsigned hi=(chr_outer&bit)?0x100U:0U;
            set_chr_bank(physical,bank_1k,int((pages[i]&255U)|hi));
        }
    }
    void old_chip_clock(){
        if(counter_just_clocked) --counter_just_clocked;
        const unsigned old=irq_ctr;
        if(old==0U)irq_ctr=irq_latch;
        else irq_ctr=uint8_t(old-1U);
        /* MMC3A/NEC does NOT trigger on a latch=0 reload. */
        if(old!=0U && irq_ctr==0U)irq_flag=irq_enabled;
    }
public:
    AuroraVn46Mapper012():chr_outer(0){}
    virtual void reset_state(){Mapper004::reset_state();chr_outer=0;}
    virtual void update_chr_banks(){map_chr();}
    virtual void apply_mapping(){
        Mapper004::apply_mapping();
        intercept_writes(0x4100,0x0f00);
    }
    virtual bool write_intercepted(nes_time_t,nes_addr_t a,int data){
        if(a>=0x4100U && a<0x5000U && (a&0x100U)){
            chr_outer=uint8_t(data);map_chr();return true;
        }
        return false;
    }
    virtual void a12_clocked(){
        old_chip_clock();if(irq_enabled)irq_changed();
    }
    virtual void run_until(nes_time_t end){
        if(next_time<0)next_time=0;
        const bool clock=scanline_a12_clocking();
        const nes_time_t finish=end*ppu_overclock;
        while(next_time<finish && next_time<=last_scanline){
            if(clock)old_chip_clock();
            next_time+=Nes_Ppu::scanline_len;
        }
    }
    virtual nes_time_t next_irq(nes_time_t now){
        run_until(now);
        if(!irq_enabled)return no_irq;
        if(irq_flag)return 0;
        if(!scanline_a12_clocking())return no_irq;
        /* If latch and counter are both zero, the early MMC3A never fires. */
        if(irq_ctr==0U && irq_latch==0U)return no_irq;
        const unsigned remaining=irq_ctr?unsigned(irq_ctr)-1U:unsigned(irq_latch);
        const long target=long(next_time)+long(remaining)*Nes_Ppu::scanline_len;
        if(target>last_scanline)return no_irq;
        return nes_time_t(target/ppu_overclock+1);
    }
    virtual void save_state(mapper_state_t& out){
        Mapper004::save_state(out);
        if(out.size>=0 && unsigned(out.size)+1U<=sizeof out.data){
            out.data[out.size++]=chr_outer;
        }
    }
    virtual void read_state(mapper_state_t const& in){
        /* Avoid the base read_state()'s premature apply_mapping() call. */
        unsigned char bytes[sizeof(mmc3_state_t)+1U];
        memset(bytes,0,sizeof bytes);
        in.read(bytes,sizeof bytes);
        mmc3_state_t* core=this;
        memcpy(core,bytes,sizeof(mmc3_state_t));
        chr_outer=bytes[sizeof(mmc3_state_t)];
        /* Nes_Mapper::load_state() applies the complete mapping after this. */
    }
};
