#pragma once
/* AURORA_QN_090_160_V4_20261018
 * Independent implementations; mapper 160 is Sachen SA009, NOT a JY/90 alias.
 * Register/banking behavior adapted from Mesen2 JyCompany and FCEUX sachen.cpp.
 * Mapper 90 rendering IRQ sources 1/2 use scanline-level approximation, as
 * QuickNES does not notify a mapper for every PPU pattern/nametable fetch.
 */
#include "Nes_Mapper.h"
#include <stdint.h>
#include <string.h>

class Mapper090 : public Nes_Mapper {
    struct state_t {
        uint8_t prg[4],chr_lo[8],chr_hi[8];
        uint8_t d000,d001,d002,d003;
        uint8_t mul[2],reg_ram;
        uint8_t irq_enabled,irq_mode,irq_pre,irq_counter,irq_xor,irq_funky;
        uint8_t irq_pending;
    } s;
    nes_time_t cpu_cursor;
    int32_t next_ppu_scanline;
    enum { first_ppu_scanline = 20 * Nes_Ppu::scanline_len + 268,
           last_ppu_scanline = first_ppu_scanline + 240 * Nes_Ppu::scanline_len };

    static unsigned invert_prg(int v) {
        // Mapper 90 bit-reversed banking; bit 3 is not connected in this mode.
        return ((v & 1) << 6) | ((v & 2) << 4) | ((v & 4) << 2) |
               ((v & 0x10) >> 2) | ((v & 0x20) >> 4) | ((v & 0x40) >> 6);
    }
    int prg_page(int i) const {
        int b=s.prg[i]&0x7f;
        if((s.d000&3)==3) b=(int)invert_prg(b);
        return (b & (((s.d000&3)==3)?0x7f:0x3f)) | ((s.d003&6)<<5);
    }
    int fixed_page(int b) const {
        return (b&0x3f)|((s.d003&6)<<5);
    }
    void prg_mapping() {
        const int pmode=s.d000&3;
        const int select_last=(s.d000&4)!=0;
        const int outer=(s.d003&6)<<5;
        if(pmode==0) {
            // PRG32 register selects 32 KiB banks, not raw 8 KiB pages.
            const int b=select_last?(((prg_page(3)&15)<<2)|outer):fixed_page(0x3c);
            for(int i=0;i<4;i++)set_prg_bank(0x8000+i*0x2000,bank_8k,b+i);
            if(s.d000&0x80)set_prg_bank(0x6000,bank_8k,(((prg_page(3)&15)<<2)+3)|outer);
        } else if(pmode==1) {
            const int b0=((prg_page(1)&31)<<1)|outer;
            const int b1=select_last?((prg_page(3)&31)<<1)|outer:fixed_page(0x3e);
            for(int i=0;i<2;i++)set_prg_bank(0x8000+i*0x2000,bank_8k,b0+i);
            for(int i=0;i<2;i++)set_prg_bank(0xc000+i*0x2000,bank_8k,b1+i);
            if(s.d000&0x80)set_prg_bank(0x6000,bank_8k,(((prg_page(3)&31)<<1)+1)|outer);
        } else {
            for(int i=0;i<3;i++)set_prg_bank(0x8000+i*0x2000,bank_8k,prg_page(i));
            set_prg_bank(0xe000,bank_8k,select_last?prg_page(3):fixed_page(0x3f));
            if(s.d000&0x80)set_prg_bank(0x6000,bank_8k,prg_page(3));
        }
        if(!(s.d000&0x80))enable_sram(false);
    }
    int chr_register(int i) const {
        if((s.d003&0x80) && ((s.d000>>3)&3)>=2 && (i==2 || i==3))i-=2;
        int bank=s.chr_lo[i]|(s.chr_hi[i]<<8);
        if((s.d003&0x20)==0) {
            static const int mask[4]={0x1f,0x3f,0x7f,0xff};
            static const int shift[4]={5,6,7,8};
            const int mode=(s.d000>>3)&3;
            const int block=((s.d003&0x18)>>2)|(s.d003&1);
            bank=(s.chr_lo[i]&mask[mode])|(block<<shift[mode]);
        }
        return bank;
    }
    void chr_mapping() {
        switch((s.d000>>3)&3) {
        case 0:
            set_chr_bank(0,bank_8k,chr_register(0));break;
        case 1:
            set_chr_bank(0,bank_4k,chr_register(0));
            set_chr_bank(0x1000,bank_4k,chr_register(4));break;
        case 2:
            for(int i=0;i<4;i++)set_chr_bank(i*0x800,bank_2k,chr_register(i*2));
            break;
        case 3:
            for(int i=0;i<8;i++)set_chr_bank(i*0x400,bank_1k,chr_register(i));
            break;
        }
    }
    void mirroring() {
        if(cart().mirroring()&8) return;
        switch(s.d001&3) {
        case 0:mirror_vert();break;
        case 1:mirror_horiz();break;
        case 2:mirror_single(0);break;
        case 3:mirror_single(1);break;
        }
    }
    bool ppu_a12_active() const {
        if(!(emu().ppu.w2001&0x18))return false;
        const int ctrl=emu().ppu.w2000;
        if(ctrl&0x20)return true; // 8x16 sprites use both pattern regions
        return (ctrl&0x18)==0x08 || (ctrl&0x18)==0x10;
    }
    unsigned clocks_to_irq() const {
        const int dir=(s.irq_mode>>6)&3;
        if(dir!=1 && dir!=2)return 0;
        const unsigned span=(s.irq_mode&4)?8:256;
        const unsigned pre=s.irq_pre&(span-1);
        const unsigned first=(dir==1)?(span-pre):(pre+1);
        const unsigned count=(dir==1)?(256u-s.irq_counter):(1u+s.irq_counter);
        return first + (count-1)*span;
    }
    void tick_irq(unsigned n) {
        if(n==0)return;
        const unsigned dir=(s.irq_mode>>6)&3;
        if(dir!=1 && dir!=2)return;
        const unsigned span=(s.irq_mode&4)?8:256;
        const unsigned mask=span-1;
        const unsigned prev=s.irq_pre&mask;
        unsigned crosses;
        if(dir==1) {
            crosses=(prev+n)/span;
            s.irq_pre=(uint8_t)((s.irq_pre&~mask)|((prev+n)&mask));
        } else {
            crosses=(n+mask-prev)/span;
            s.irq_pre=(uint8_t)((s.irq_pre&~mask)|((prev+span-(n%span))&mask));
        }
        if(!crosses)return;
        const unsigned threshold=dir==1?(256u-s.irq_counter):(1u+s.irq_counter);
        if(crosses>=threshold && s.irq_enabled && !s.irq_pending) {
            s.irq_pending=1;
            irq_changed();
        }
        s.irq_counter=(uint8_t)(dir==1?(unsigned)s.irq_counter+crosses:(unsigned)s.irq_counter-crosses);
    }
    void ack_irq() {s.irq_pending=0;irq_changed();}
public:
    Mapper090():cpu_cursor(0),next_ppu_scanline(first_ppu_scanline) {
        register_state(&s,sizeof s);
    }
    void reset_state() {
        memset(&s,0,sizeof s);
        s.d003=0x20; // first power-on uses direct CHR register pages
        cpu_cursor=0;
        next_ppu_scanline=first_ppu_scanline;
    }
    void apply_mapping() {
        prg_mapping();chr_mapping();mirroring();
        intercept_reads(0x5000,0x1000);
        intercept_writes(0x5000,0x1000);
        // Fallback for $6000-$7FFF writes: capture CPU-write IRQ source without
        // changing other core mappers' CPU interception tables.
        intercept_writes(0x6000,0x2000);
        cpu_cursor=0;
        next_ppu_scanline=first_ppu_scanline;
    }
    int read(nes_time_t t,nes_addr_t a) {
        run_until(t);
        if(a>=0x5000 && a<0x6000) {
            switch(a&0xf803) {
            case 0x5000:return 0; // solder pad / configurable DIP = 0
            case 0x5800:return s.mul[0]*s.mul[1]&0xff;
            case 0x5801:return (s.mul[0]*s.mul[1]>>8)&0xff;
            case 0x5803:return s.reg_ram;
            }
        }
        return -1;
    }
    bool write_intercepted(nes_time_t t,nes_addr_t a,int v) {
        if(a<0x5000 || a>=0x8000)return false;
        run_until(t);
        if((s.irq_mode&3)==3)tick_irq(1);
        if(a<0x6000) {
            switch(a&0xf803) {
            case 0x5800:s.mul[0]=(uint8_t)v;return true;
            case 0x5801:s.mul[1]=(uint8_t)v;return true;
            case 0x5803:s.reg_ram=(uint8_t)v;return true;
            }
            return true; // $5000-$5fff is ASIC space
        }
        return false; // preserve stock SRAM semantics
    }
    void write(nes_time_t t,nes_addr_t a,int v) {
        run_until(t);
        if((s.irq_mode&3)==3)tick_irq(1);
        switch(a&0xf000) {
        case 0x8000: if((a&7)<=7){s.prg[a&3]=(uint8_t)(v&0x7f);prg_mapping();}break;
        case 0x9000: s.chr_lo[a&7]=(uint8_t)v;chr_mapping();break;
        case 0xa000: s.chr_hi[a&7]=(uint8_t)v;chr_mapping();break;
        case 0xb000: // nametable ROM and custom NT disabled on mapper 90
            break;
        case 0xc000:
            switch(a&7) {
            case 0:s.irq_enabled=v&1;if(!s.irq_enabled)ack_irq();break;
            case 1:s.irq_mode=(uint8_t)v;break;
            case 2:s.irq_enabled=0;ack_irq();break;
            case 3:s.irq_enabled=1;break;
            case 4:s.irq_pre=(uint8_t)(v^s.irq_xor);break;
            case 5:s.irq_counter=(uint8_t)(v^s.irq_xor);break;
            case 6:s.irq_xor=(uint8_t)v;break;
            case 7:s.irq_funky=(uint8_t)v;break;
            }
            irq_changed();break;
        case 0xd000:
            switch(a&3) {
            case 0:s.d000=(uint8_t)v;prg_mapping();chr_mapping();break;
            case 1:s.d001=(uint8_t)v;mirroring();break;
            case 2:s.d002=(uint8_t)v;break;
            case 3:s.d003=(uint8_t)v;prg_mapping();chr_mapping();break;
            }break;
        default:break;
        }
    }
    virtual bool ppu_ctrl_mask_irq_sensitive() const {
        return (s.irq_mode&3)==1 || (s.irq_mode&3)==2;
    }
    void a12_clocked() {if((s.irq_mode&3)==1)tick_irq(1);}
    void run_until(nes_time_t t) {
        if(t<cpu_cursor)cpu_cursor=t;
        if((s.irq_mode&3)==0 && t>cpu_cursor)tick_irq((unsigned)(t-cpu_cursor));
        cpu_cursor=t;
        const int source=s.irq_mode&3;
        if(source==1 || source==2) {
            const int32_t to=(int32_t)(t*ppu_overclock);
            const unsigned reads=(source==1)?8u:170u;
            while(next_ppu_scanline<to && next_ppu_scanline<=last_ppu_scanline) {
                if(source==2?(emu().ppu.w2001&0x18):ppu_a12_active())tick_irq(reads);
                next_ppu_scanline+=Nes_Ppu::scanline_len;
            }
        }
    }
    void end_frame(nes_time_t frame_time) {
        run_until(frame_time);
        cpu_cursor=0;
        next_ppu_scanline=first_ppu_scanline;
    }
    nes_time_t next_irq(nes_time_t present) {
        run_until(present);
        if(s.irq_pending)return 0;
        if(!s.irq_enabled)return no_irq;
        const unsigned count=clocks_to_irq();
        if(!count)return no_irq;
        if((s.irq_mode&3)==0) return present+(nes_time_t)count;
        if((s.irq_mode&3)==1 || (s.irq_mode&3)==2) {
            if((s.irq_mode&3)==1?!ppu_a12_active():!(emu().ppu.w2001&0x18))return no_irq;
            const unsigned per=(s.irq_mode&3)==1?8:170;
            const unsigned scans=(count+per-1)/per;
            const int32_t when=next_ppu_scanline+(int32_t)(scans-1)*Nes_Ppu::scanline_len;
            if(when>last_ppu_scanline)return no_irq;
            return (nes_time_t)(when/ppu_overclock+1);
        }
        // Source 3: CPU-write count; no predictable future CPU writes.
        return no_irq;
    }
};

/* Mapper 160: Sachen SA009 (FCEUX sachen.cpp). NOT an alias for mapper 90. */
class Mapper160 : public Nes_Mapper {
    uint8_t latched;
public:
    Mapper160() {register_state(&latched,1);}
    void reset_state() {latched=0;}
    void apply_mapping() {
        set_prg_bank(0x8000,bank_32k,0);
        set_chr_bank(0,bank_8k,latched&1);
        intercept_writes(0x4100,0x1f00);
    }
    bool write_intercepted(nes_time_t,nes_addr_t a,int v) {
        if(a<0x4100 || a>0x5fff || (a&0x100)==0)return false;
        latched=(uint8_t)v;
        set_chr_bank(0,bank_8k,latched&1);
        return true;
    }
    void write(nes_time_t,nes_addr_t,int) {} // SA009 has no $8000+ register
};
