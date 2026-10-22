#ifndef AURORA_VN62_MAPPER540_CORE_STAGING_HPP
#define AURORA_VN62_MAPPER540_CORE_STAGING_HPP
/* SNESticleAurora/QuickNES vN59: mapper 540 UNL-82112C staged ASIC model.
 * NEVER register as finished: FDS expansion audio / real CPU and PPU scheduler
 * integration are absent. Based on FCEUmm mapper359.c behavioral reference:
 * GPL-2.0-or-later, copyrights 2020, 2023-2026 negativeExponent.
 * This independent staged model does not include source from that project.
 */
#include <stdint.h>
#include <string.h>

class AuroraMapper540Stage {
public:
    enum { snapshot_size = 24 };
    struct Mapping { unsigned prg8[5]; unsigned chr2[4]; unsigned mirror[4]; };
private:
    uint8_t prg_[4], chr_[8], regs_[4];
    uint8_t reload_, enabled_, ppu_mode_, auto_enable_, latch_, irq_count_;
    uint16_t cpu_count_; // raw two's-complement 16-bit IRQ counter
    uint8_t irq_pending_;
public:
    AuroraMapper540Stage() { reset(); }
    void reset() {
        for(unsigned i=0;i<4;++i)prg_[i]=(uint8_t)(0xfcU+i);
        for(unsigned i=0;i<8;++i)chr_[i]=(uint8_t)i;
        memset(regs_,0,sizeof regs_);regs_[1]=0x40;
        reload_=enabled_=ppu_mode_=auto_enable_=latch_=irq_count_=0;
        cpu_count_=0;irq_pending_=0;
    }
    bool write(unsigned address,unsigned data) {
        address&=65535U;data&=255U;
        if(address<0x8000U || address>=0xd000U)return false;
        const unsigned hi=address&0xf000U;
        if(hi==0x8000U){prg_[address&3U]=(uint8_t)data;return true;}
        if(hi==0x9000U){regs_[address&3U]=(uint8_t)data;return true;}
        if(hi==0xa000U||hi==0xb000U) {
            unsigned i=((address>>10)&4U)|(address&3U);
            chr_[i]=(uint8_t)data;return true;
        }
        if(hi!=0xc000U)return false;
        switch(address&3U) {
        case 0:
            if(auto_enable_)enabled_=0;
            cpu_count_=(uint16_t)((cpu_count_&0xff00U)|data);
            irq_pending_=0;break;
        case 1:
            if(auto_enable_)enabled_=1;
            cpu_count_=(uint16_t)((cpu_count_&0x00ffU)|(data<<8));
            reload_=1;latch_=(uint8_t)data;irq_pending_=0;break;
        case 2:
            enabled_=(uint8_t)(data&1U);
            ppu_mode_=(uint8_t)((data>>1)&1U);
            auto_enable_=(uint8_t)((data>>2)&1U);
            irq_pending_=0;break;
        default:enabled_=(uint8_t)(data&1U);irq_pending_=0;break;
        }
        return true;
    }
    // Call with elapsed CPU cycles from a verified scheduler. No fabricated cycles.
    void cpu_cycles(unsigned cycles) {
        if(ppu_mode_ || !enabled_ || !cpu_count_ || !cycles)return;
        // FCEUmm's register is int16_t. Preserve the hardware-width wrap,
        // including negative values after an overshooting CPU-cycle batch.
        // vN92: the CPU scheduler is allowed to submit a large cycle batch.
        // The previous modulo-65536 subtraction could miss a crossing of zero
        // (for example, count=1, cycles=65536). Latch the crossing BEFORE
        // applying the hardware-width wrap; keep the 16-bit counter ABI.
        const uint16_t before=cpu_count_;
        if ((before & 0x8000U) || cycles >= (unsigned)before)
            irq_pending_=1;
        cpu_count_=(uint16_t)(before-(uint16_t)cycles);
    }
    // This is ONE mapper PPU IRQ clock from a VERIFIED source, not every
    // A12 edge. The upstream handler uses a per-scanline callback and the
    // precise correspondence on QuickNES hardware remains unverified.
    void ppu_irq_clock() {
        if(!ppu_mode_)return;
        if(!irq_count_ || reload_)irq_count_=latch_;
        else --irq_count_;
        if(!irq_count_ && enabled_)irq_pending_=1;
        reload_=0;
    }
    bool pending_irq() const {return irq_pending_!=0;}
    unsigned cpu_count() const {return cpu_count_;}
    bool cpu_irq_active() const {return enabled_ && !ppu_mode_ && !irq_pending_ && cpu_count_!=0;}
    unsigned ppu_count() const {return irq_count_;}
    bool uses_ppu_irq() const {return ppu_mode_!=0;}
    void map(Mapping &out) const {
        static const unsigned masks[4]={0x3fU,0x1fU,0x2fU,0x0fU};
        const unsigned mask=masks[regs_[1]&3U];
        const unsigned base=(unsigned(regs_[0]&0x38U)<<1);
        // FCEUmm mapper359.c SyncPRG: $6000 uses register 3;
        // $8000/$A000/$C000 use registers 0,1,2 respectively.
        out.prg8[0]=base|(unsigned(prg_[3])&mask);
        for(unsigned i=0;i<3;++i)out.prg8[i+1]=base|(unsigned(prg_[i])&mask);
        out.prg8[4]=base|(255U&mask);
        // mapper 540: CHR selects 2 KiB banks 0, 1, 6, 7.
        out.chr2[0]=chr_[0];out.chr2[1]=chr_[1];
        out.chr2[2]=chr_[6];out.chr2[3]=chr_[7];
        const unsigned mir=regs_[2]&3U;
        for(unsigned i=0;i<4;++i)
            out.mirror[i]=mir==0?(i&1U):mir==1?(i>>1):mir==2?0U:1U;
    }
    // Endian-stable serialization; high byte [23] reserved for stage only.
    void save(uint8_t out[snapshot_size]) const {
        memcpy(out,prg_,4);memcpy(out+4,chr_,8);memcpy(out+12,regs_,4);
        out[16]=reload_;out[17]=enabled_;out[18]=ppu_mode_;
        out[19]=auto_enable_;out[20]=latch_;out[21]=irq_count_;
        out[22]=(uint8_t)cpu_count_;out[23]=(uint8_t)(cpu_count_>>8);
        // irq_pending_ is an emulator IRQ line, not in the upstream snapshot.
        // Stage stores that line separately in MapperState::irq_pending.
    }
    void load(const uint8_t in[snapshot_size]) {
        memcpy(prg_,in,4);memcpy(chr_,in+4,8);memcpy(regs_,in+12,4);
        reload_=in[16]&1U;enabled_=in[17]&1U;ppu_mode_=in[18]&1U;
        auto_enable_=in[19]&1U;latch_=in[20];irq_count_=in[21];
        cpu_count_=(uint16_t)(in[22]|(unsigned(in[23])<<8));
        irq_pending_=0; // IRQ line reestablished separately by caller
    }
    void set_irq_line(bool pending) {irq_pending_=(uint8_t)(pending!=0);}
};
#endif