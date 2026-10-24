#ifndef AURORA_VN67_JYASIC_QUICKNES_CANDIDATE_HPP
#define AURORA_VN67_JYASIC_QUICKNES_CANDIDATE_HPP
/* JY ASIC / NES2 mapper 35 and 209 native QuickNES candidate.
 * No mapper 540 code. Original fork is not modified by this header.
 * FCEUmm mapper209.c, jyasic.c consulted for hardware behavior.
 * Compatibility still needs ROM/PS2DEV validation. No physical DIP support.
 */
#include "Nes_Mapper.h"
#include "Nes_Core.h"
#include "aurora_vn71_jyasic.hpp"
#include <string.h>

class AuroraJYMapper35_209 : public Nes_Mapper {
    AuroraJYASICStage chip_;
    unsigned char state_[AuroraJYASICStage::snapshot_size+4];
    nes_time_t cpu_cursor_;
    nes_time_t scanline_next_;
    int variant_;
    enum { scan_first=20*341+268, scan_final=scan_first+240*341 };
    bool scanline_enabled() const {return (emu().ppu.w2001&0x18)!=0;}
    void sync_prg() {
        AuroraJYASICStage::Mapping m;chip_.map(m);
        for(unsigned i=0;i<4U;++i)
            set_prg_bank((nes_addr_t)(0x8000U+i*0x2000U),bank_8k,(int)m.prg8[i]);
    }
    void sync_wram() {
        AuroraJYASICStage::Mapping m;chip_.map(m);
        if(m.wram_is_rom) {
            enable_sram(false);
            set_prg_bank(0x6000,bank_8k,(int)m.wram_rom_bank);
        } else if((variant_==35 && !cart().nes2_header()) ||
                  cart().prg_volatile_ram_size()>0 ||
                  cart().prg_nonvolatile_ram_size()>0) {
            enable_sram(true);
        } else enable_sram(false);
    }
    void sync_chr() {
        AuroraJYASICStage::Mapping m;chip_.map(m);
        for(unsigned i=0;i<8U;++i) {
            const nes_addr_t dst=(nes_addr_t)(i*0x400U);
            // Normal 8 KiB CHR-RAM uses Nes_Ppu_Impl::set_chr_bank(),
            // which converts the 1 KiB bank index into a BYTE offset.
            // set_chr_ram_bank() is for special CHR-RAM and its ordinary-RAM
            // fallback passes the numeric bank to set_chr_bank() as bytes;
            // that maps nearly every nonzero bank to page zero.
            set_chr_bank(dst,bank_1k,(int)m.chr1[i]);
        }
        // vN85: $2007 writes use the same physical 1 KiB bank as $2007 reads.
        // Off by default, and never affects cartridges with CHR-ROM.
        emu().ppu.aurora_set_jy_chr_ram_banking(!cart().chr_size());
        // vN71 JY-only CHR-RAM write policy (default OFF for other carts).
        emu().ppu.aurora_set_jy_chr_ram_write_protect(!cart().chr_size() && m.chr_write_protected);
    }
    // PPU callback can originate inside rendering. Do not recursively call
    // set_chr_bank() (which flushes render_until()) from that context.
    void sync_chr_during_ppu_hook() {
        AuroraJYASICStage::Mapping m;chip_.map(m);
        for(unsigned i=0;i<8U;++i) {
            const nes_addr_t addr=(nes_addr_t)(i*0x400U);
            // Physical PPU callback: update mapping directly, avoiding
            // render_until() reentry. Offset is always measured in bytes.
            emu().ppu.set_chr_bank(addr,0x400,(long)m.chr1[i]*0x400L);
        }
    }
    void sync_nt() {
        AuroraJYASICStage::Mapping m;chip_.map(m);
        int pages[4];
        for(unsigned i=0;i<4U;++i)pages[i]=(m.nt_source[i]==AuroraJYASICStage::CHR_ROM)?0:(int)m.nt_bank[i];
        mirror_manual(pages[0],pages[1],pages[2],pages[3]);
        for(unsigned i=0;i<4U;++i)
            if(m.nt_source[i]==AuroraJYASICStage::CHR_ROM && cart().chr_size())
                mirror_chr((int)i,(int)m.nt_bank[i]);
    }
    void sync_all() {sync_prg();sync_wram();sync_chr();sync_nt();}
    void sync_register_write(nes_time_t t,nes_addr_t addr,int data) {
        run_until(t);
        const bool old=chip_.pending_irq();
        if(!chip_.cpu_write(addr,(unsigned)data))return;
        if(addr>=0x8000U && addr<0x8800U) {sync_prg();sync_wram();}
        else if(addr>=0x9000U && addr<0x9800U)sync_chr();
        else if(addr>=0xa000U && addr<0xa800U)sync_chr();
        else if(addr>=0xb000U && addr<0xb800U)sync_nt();
        else if(addr>=0xd000U && addr<0xd800U)sync_all();
        if(old!=chip_.pending_irq() || (addr>=0xc000U && addr<0xd000U))irq_changed();
    }
    void advance(nes_time_t until) {
        if(until<=cpu_cursor_)return;
        const bool old=chip_.pending_irq();
        const unsigned long delta=(unsigned long)(until-cpu_cursor_);
        if(chip_.irq_source()==0U && chip_.irq_is_enabled()) {
            // Account for every CPU clock, even after IRQ assertion.
            // A pending IRQ does not freeze the JYASIC prescaler/counter.
            unsigned long clocks=delta;
            while(clocks) {
                const unsigned chunk=clocks>0xffffffffUL?0xffffffffU:(unsigned)clocks;
                chip_.on_cpu_cycles_fast(chunk);
                clocks-=chunk;
            }
        }
        // FCEUmm JYASIC approximates 8 rises each visible scanline for
        // A12 mode. Keep this opt-in to 35/209 and do not globally rewrite PPU.
        {
            // Even when A12 IRQ is inactive, move the scheduling cursor: a
            // mid-frame change to A12 mode must not replay old scanlines.
            const nes_time_t ppu_time=until*ppu_overclock;
            while(scanline_next_<ppu_time && scanline_next_<=scan_final) {
                if(chip_.irq_source()==1U && scanline_enabled())
                    for(unsigned i=0;i<8U;++i)chip_.on_ppu_a12_rise();
                scanline_next_+=Nes_Ppu::scanline_len;
            }
        }
        cpu_cursor_=until;
        if(old!=chip_.pending_irq())irq_changed();
    }
public:
    explicit AuroraJYMapper35_209(int v) : cpu_cursor_(0),scanline_next_(scan_first),variant_(v) {
        memset(state_,0,sizeof state_);
        register_state(state_,sizeof state_);
    }
    virtual void reset_state() {
        chip_.reset();cpu_cursor_=0;scanline_next_=scan_first;
    }
    virtual bool ppu_ctrl_mask_irq_sensitive() const {return true;}
    virtual bool aurora_observe_all_cpu_writes() const {return true;}
    virtual void aurora_cpu_write_event(nes_time_t t,nes_addr_t a,int v) {
        (void)a;(void)v;
        advance(t);
        bool old=chip_.pending_irq();
        chip_.on_cpu_write_bus();
        if(old!=chip_.pending_irq())irq_changed();
    }
    virtual void apply_mapping() {
        sync_all();
        intercept_reads(0x5000,0x1000);
        intercept_writes(0x5000,0x1000);
    }
    virtual void write(nes_time_t t,nes_addr_t a,int v) {sync_register_write(t,a,v);}
    virtual bool write_intercepted(nes_time_t t,nes_addr_t a,int v) {
        if(a<0x5000U || a>=0x6000U)return false;
        // The ASIC ALU is physically writable at $5000-$5FFF.
        sync_register_write(t,a,v);return true;
    }
    virtual int read(nes_time_t t,nes_addr_t a) {
        advance(t);unsigned v=0;
        return chip_.read_alu(a,v)?(int)v:-1;
    }
    virtual bool needs_vram_address_hook() const {return true;}
    virtual void vram_address_changed(nes_addr_t a) {
        AuroraJYASICStage::Mapping before,after;
        chip_.map(before);
        const bool pending=chip_.pending_irq();
        chip_.on_ppu_address((unsigned)a);
        chip_.map(after);
        bool different=false;
        for(unsigned i=0;i<8U;++i) if(before.chr1[i]!=after.chr1[i])different=true;
        if(different)sync_chr_during_ppu_hook();
        if(pending!=chip_.pending_irq())irq_changed();
    }
    virtual void run_until(nes_time_t t) {advance(t);}
    virtual nes_time_t next_irq(nes_time_t t) {
        advance(t);
        if(chip_.pending_irq())return t;
        unsigned clocks=chip_.cpu_clocks_until_irq();
        if(clocks!=~0U)return cpu_cursor_+(nes_time_t)clocks;
        if(chip_.irq_source()==1U && chip_.irq_is_enabled() && scanline_enabled()) {
            AuroraJYASICStage candidate=chip_;
            for(nes_time_t when=scanline_next_;when<=scan_final;when+=Nes_Ppu::scanline_len) {
                for(unsigned i=0;i<8U;++i)candidate.on_ppu_a12_rise();
                if(candidate.pending_irq())return when/ppu_overclock+1;
            }
        }
        return no_irq;
    }
    virtual void end_frame(nes_time_t t) {
        advance(t);cpu_cursor_-=t;scanline_next_=scan_first;
    }
    virtual void save_state(mapper_state_t& out) {
        advance(mapper_clock());
        chip_.save(state_);
        unsigned v=(unsigned)scanline_next_;
        for(unsigned i=0;i<4U;++i)state_[AuroraJYASICStage::snapshot_size+i]=(unsigned char)(v>>(8U*i));
        Nes_Mapper::save_state(out);
    }
    virtual void read_state(mapper_state_t const& in) {
        memset(state_,0,sizeof state_);
        in.read(state_,sizeof state_);
        chip_.load(state_);cpu_cursor_=0;
        unsigned v=0;
        for(unsigned i=0;i<4U;++i)v|=(unsigned)state_[AuroraJYASICStage::snapshot_size+i]<<(8U*i);
        if(v>=scan_first && v<=scan_final+Nes_Ppu::scanline_len &&
           ((v-scan_first)%Nes_Ppu::scanline_len)==0U)scanline_next_=v;
        else scanline_next_=scan_first;
        sync_all();
    }
};
class Mapper035 : public AuroraJYMapper35_209 {public: Mapper035():AuroraJYMapper35_209(35) {}};
class Mapper209 : public AuroraJYMapper35_209 {public: Mapper209():AuroraJYMapper35_209(209) {}};
#endif
