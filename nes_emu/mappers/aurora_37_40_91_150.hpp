#pragma once
/* AURORA_QN_37_40_91_150_FINAL
 * Mapper 37: MMC3 multicart block selector (Mesen2 MMC3_37)
 * Mapper 40: FDS conversion/Whirlwind timer (Mesen2 Mapper40)
 * Mapper 91: JY Company 2K CHR / MMC3-style 8-scanline IRQ (Mesen2/FCEUX)
 * Mapper 150: Sachen 74LS374N with 4100/4101 command/data latches (Mesen2)
 * No source from other mappers modified; all mapper state is local.
 */
#include "Nes_Mapper.h"
#include "mapper004.hpp"
#include <stdint.h>
#include <string.h>

class Mapper037 : public Mapper004 {
    uint8_t selected_block;
    int translate_prg(int b) const {
        if (b < 0) {
            int pages = (int)(cart().prg_size()/0x2000);
            if (pages <= 0) pages = 1;
            b = (b % pages + pages) % pages;
        }
        switch (selected_block) {
            case 0:case 1:case 2: return b & 7;
            case 3: return (b & 7) | 8;
            case 7: return (b & 7) | 0x20;
            default: return (b & 15) | 0x10;
        }
    }
    int translate_chr(int b) const {
        return selected_block >= 4 ? (b | 0x80) : b;
    }
    void update_all_banks() {
        const int prg_swap=(mode&0x40)!=0;
        set_prg_bank(0x8000,bank_8k,translate_prg(prg_swap ? last_bank-1 : banks[6]));
        set_prg_bank(0xA000,bank_8k,translate_prg(banks[7]));
        set_prg_bank(0xC000,bank_8k,translate_prg(prg_swap ? banks[6] : last_bank-1));
        set_prg_bank(0xE000,bank_8k,translate_prg(last_bank));
        const int flip = (mode&0x80)?4:0;
        const int chr[8] = {banks[0]&~1,banks[0]|1,banks[1]&~1,banks[1]|1,
                            banks[2],banks[3],banks[4],banks[5]};
        for(int i=0;i<8;i++)
            set_chr_bank((i^flip)*0x400,bank_1k,translate_chr(chr[i]));
    }
public:
    Mapper037() : selected_block(0) {}
    void reset_state() {
        Mapper004::reset_state();
        selected_block=0;
    }
    void apply_mapping() {
        // Avoid synthetic translated writes on state-load; preserve MMC3 SRAM mode.
        if (!(cart().mirroring()&8)) {
            if(mirror&1) mirror_horiz(); else mirror_vert();
        }
        if((sram_mode&0x3f)==0x30)enable_sram(true);
        else enable_sram((sram_mode&0x80)!=0,(sram_mode&0x40)!=0);
        update_all_banks();
        start_frame();
        intercept_writes(0x6000,0x2000);
    }
    bool write_intercepted(nes_time_t t,nes_addr_t a,int v) {
        if(a>=0x6000 && a<0x8000) {
            // Work-RAM must be write enabled to latch the outer-bank selector.
            if((sram_mode&0x80) && !(sram_mode&0x40)) {
                run_until(t);
                selected_block=(uint8_t)(v&7);
                update_all_banks();
            }
            return false; // keep normal SRAM write semantics
        }
        return false;
    }
    void write(nes_time_t t,nes_addr_t a,int v) {
        Mapper004::write(t,a,v);
        if((a&0xE001)==0x8000 || (a&0xE001)==0x8001)
            update_all_banks();
    }
    void save_state(mapper_state_t&out) {
        Mapper004::save_state(out);
        if(out.size < (int)max_mapper_state_size)
            out.data[out.size++]=selected_block;
    }
    void load_state(mapper_state_t const&in) {
        /* V-AUDIT-37: decode the saved outer block BEFORE the
         * inherited load_state() invokes virtual apply_mapping().
         * Avoid mapping/rendering the wrong outer CHR page first. */
        selected_block=0;
        if(in.size>(int)sizeof(mmc3_state_t))
            selected_block=(uint8_t)(in.data[sizeof(mmc3_state_t)]&7);
        Mapper004::load_state(in);
    }
};

class Mapper040 : public Nes_Mapper {
    struct state_t { uint16_t irq_count; uint8_t bank,irq_pending; } s;
    nes_time_t cursor;
public:
    Mapper040() : cursor(0) { register_state(&s,sizeof s); }
    void reset_state(){memset(&s,0,sizeof s);cursor=0;}
    void apply_mapping(){
        set_prg_bank(0x6000,bank_8k,6);
        set_prg_bank(0x8000,bank_8k,4);
        set_prg_bank(0xA000,bank_8k,5);
        set_prg_bank(0xC000,bank_8k,s.bank);
        set_prg_bank(0xE000,bank_8k,7);
        set_chr_bank(0,bank_8k,0);
        cursor=0;
    }
    void run_until(nes_time_t t){
        if(t<=cursor)return;
        const unsigned dt=(unsigned)(t-cursor);
        cursor=t;
        if(!s.irq_count)return;
        if(dt >= s.irq_count){
            s.irq_count=0;
            if(!s.irq_pending){s.irq_pending=1;irq_changed();}
        }else s.irq_count=(uint16_t)(s.irq_count-dt);
    }
    nes_time_t next_irq(nes_time_t t){
        run_until(t);
        if(s.irq_pending)return 0;
        return s.irq_count ? t+s.irq_count : no_irq;
    }
    void end_frame(nes_time_t t){run_until(t);cursor=0;}
    void write(nes_time_t t,nes_addr_t a,int v){
        run_until(t);
        switch(a&0xE000){
        case 0x8000:s.irq_count=0;s.irq_pending=0;irq_changed();break;
        case 0xA000:s.irq_count=4096;s.irq_pending=0;irq_changed();break;
        case 0xE000:s.bank=(uint8_t)v;set_prg_bank(0xC000,bank_8k,s.bank);break;
        }
    }
};

class Mapper091 : public Mapper004 {
    void remap(){
        for(int i=0;i<4;i++)set_chr_bank(i*0x800,bank_2k,banks[i]);
        set_prg_bank(0x8000,bank_8k,banks[6]);
        set_prg_bank(0xA000,bank_8k,banks[7]);
        set_prg_bank(0xC000,bank_8k,last_bank-1);
        set_prg_bank(0xE000,bank_8k,last_bank);
    }
public:
    void reset_state(){
        Mapper004::reset_state();
        for(int i=0;i<4;i++)banks[i]=0;
        banks[6]=0;banks[7]=1;
    }
    void apply_mapping(){
        remap();
        start_frame();
        intercept_writes(0x6000,0x2000);
    }
    bool write_intercepted(nes_time_t t,nes_addr_t a,int v){
        if(a<0x6000 || a>=0x8000)return false;
        switch(a&0x7003){
        case 0x6000:case 0x6001:case 0x6002:case 0x6003:
            banks[a&3]=(uint8_t)v;
            set_chr_bank((a&3)*0x800,bank_2k,v);
            return true;
        case 0x7000:case 0x7001:
            banks[6+(a&1)]=(uint8_t)v;
            set_prg_bank(0x8000+(a&1)*0x2000,bank_8k,v);
            return true;
        case 0x7002:
            run_until(t);write_irq(0xE000,0);return true;
        case 0x7003:
            run_until(t);
            write_irq(0xC000,7);
            write_irq(0xC001,0);
            write_irq(0xE001,0);
            return true;
        }
        return false;
    }
    void write(nes_time_t,nes_addr_t,int) { /* No register decoder above $7FFF */ }
};

class Mapper150 : public Nes_Mapper {
    struct state_t { uint8_t regs[8],command; } s;
    void banks(){
        set_prg_bank(0x8000,bank_32k,s.regs[5]&3);
        set_chr_bank(0,bank_8k,((s.regs[4]&1)<<2)|(s.regs[6]&3));
        if(cart().mirroring()&8)return;
        switch((s.regs[7]>>1)&3){
        case 0:mirror_manual(0,0,0,1);break;
        case 1:mirror_horiz();break;
        case 2:mirror_vert();break;
        case 3:mirror_single(0);break;
        }
    }
public:
    Mapper150(){register_state(&s,sizeof s);}
    void reset_state(){memset(&s,0,sizeof s);}
    void apply_mapping(){
        banks();
        /* V-AUDIT-150: the $4100/$4101 ASIC decoder is mirrored
         * through $7FFF (Mesen2 Sachen74LS374N RegisterEndAddress).
         * Cover $6100/$6101 etc., not just $4100-$5FFF. */
        intercept_reads(0x4100,0x3f00);
        intercept_writes(0x4100,0x3f00);
    }
    int read(nes_time_t,nes_addr_t a){
        if((a&0xC101)==0x4101){
            // Fixed default DIP position (0); D3-D7 are unconnected/open-bus.
            return ((a>>8)&0xF8)|(s.regs[s.command]&7);
        }
        return -1;
    }
    bool write_intercepted(nes_time_t,nes_addr_t a,int v){
        switch(a&0xC101){
        case 0x4100:s.command=(uint8_t)(v&7);return true;
        case 0x4101:s.regs[s.command]=(uint8_t)(v&7);banks();return true;
        }
        return false;
    }
    void write(nes_time_t,nes_addr_t,int){}
};
