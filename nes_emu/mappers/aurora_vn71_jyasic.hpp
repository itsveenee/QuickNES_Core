#pragma once
/* SNESticleAurora QuickNES / vN71 — JYASIC mapper logic, shared by 35/209.
 * DO NOT register mapper 35/209 with this header; render-time PPU fetch and
 * CPU-wide write clocks are not wired to the QuickNES core yet.
 * Derived independently from the behavioral specification in FCEUmm's
 * src/mappers/hw/jyasic.c and src/mappers/mapper209.c.
 * FCEUmm reference copyright: Xodnizel, CaH4e3, Libretro Team,
 * negativeExponent. Keep GPL-2.0-or-later license obligations if incorporating
 * this implementation with GPL-licensed code.
 * No DIP switches, keyboards, accessories, or physical selection inputs.
 */
#include <stdint.h>
#include <string.h>

class AuroraJYASICStage {
public:
    enum { snapshot_size = 46 };
    enum NtSource { CIRAM = 0, CHR_ROM = 1 };
    struct Mapping {
        unsigned prg8[4];
        unsigned chr1[8];
        unsigned wram_rom_bank;
        bool wram_is_rom;
        NtSource nt_source[4];
        unsigned nt_bank[4];
        bool chr_write_protected;
    };

private:
    uint8_t mode[4], prg[4], mul[2], adder, test, latch[2];
    uint16_t chr[8], nt[4];
    uint8_t irq_control, irq_enable, irq_prescaler, irq_counter, irq_xor;
    uint8_t irq_pending;
    uint16_t last_ppu_addr;

    static unsigned reverse_7(unsigned value) {
        unsigned r = 0;
        for (unsigned i=0; i<7U; ++i) r = (r << 1) | ((value >> i) & 1U);
        return r;
    }
    unsigned transform_prg(unsigned value) const {
        return (((unsigned(mode[3]) << 5) & ~0x3fU) | (value & 0x3fU));
    }
    unsigned transform_chr(unsigned value) const {
        const unsigned high=(unsigned(mode[3]) << 6) & 0x600U;
        if (mode[3] & 0x20U) return high | (value & 0x1ffU);
        return high | ((unsigned(mode[3]) << 8) & 0x100U) | (value & 0xffU);
    }
    // Each call represents exactly one hardware IRQ source clock.
    void irq_clock() {
        if (!irq_enable) return;
        unsigned mask=(irq_control & 4U) ? 7U : 255U;
        unsigned prescaler=unsigned(irq_prescaler) & mask;
        bool counter_tick=false;
        switch (irq_control & 0xc0U) {
        case 0x40U:
            ++prescaler;
            counter_tick=((prescaler & mask)==0U);
            break;
        case 0x80U:
            prescaler=(prescaler-1U)&255U;
            counter_tick=(prescaler==0U);
            break;
        default: break;
        }
        irq_prescaler=(uint8_t)((unsigned(irq_prescaler)&~mask)|(prescaler&mask));
        if (!counter_tick) return;
        if ((irq_control & 0x08U)==0U) {
            if ((irq_control & 0xc0U)==0x40U) ++irq_counter;
            else if ((irq_control & 0xc0U)==0x80U) --irq_counter;
        }
        if (((irq_control & 0xc0U)==0x40U && irq_counter==0U) ||
            ((irq_control & 0xc0U)==0x80U && irq_counter==255U)) irq_pending=1;
    }
    void write_irq(unsigned addr,unsigned data) {
        switch (addr & 7U) {
        case 0:
            irq_enable=(uint8_t)(data & 1U);
            if (!irq_enable) {irq_prescaler=0;irq_pending=0;}
            break;
        case 1: irq_control=(uint8_t)data; break;
        case 2: irq_enable=0;irq_prescaler=0;irq_pending=0;break;
        case 3: irq_enable=1;break;
        case 4: irq_prescaler=(uint8_t)(data ^ irq_xor);break;
        case 5: irq_counter=(uint8_t)(data ^ irq_xor);break;
        case 6: irq_xor=(uint8_t)data;break;
        default:break;
        }
    }
    void write_mode(unsigned addr,unsigned data) {
        const unsigned i=addr&3U;
        mode[i]=(uint8_t)data;
    }
public:
    AuroraJYASICStage() {reset();}
    void reset() {
        memset(mode,0,sizeof mode);memset(prg,0,sizeof prg);
        memset(mul,0,sizeof mul);adder=0;test=0;
        latch[0]=0;latch[1]=4;
        memset(chr,0,sizeof chr);memset(nt,0,sizeof nt);
        irq_control=0;irq_enable=0;irq_prescaler=0;irq_counter=0;
        irq_xor=0;irq_pending=0;last_ppu_addr=0;
    }
    // Register write only. Entire CPU bus write IRQ clocks are delivered
    // separately via on_cpu_write_bus(); NEVER clock a register write twice.
    void on_cpu_write_bus() { if ((irq_control & 3U) == 3U) irq_clock(); }
    // Returns true for an ASIC register write. CPU register writes must
    // not be conflated with CPU-bus clock notifications.
    bool cpu_write(unsigned addr,unsigned data) {
        addr&=0xffffU;data&=255U;
        if (addr>=0x5000U && addr<=0x5fffU) {
            switch (addr&3U) {
            case 0: mul[0]=(uint8_t)data;break;
            case 1: mul[1]=(uint8_t)data;break;
            case 2: adder=(uint8_t)(adder+data);break;
            case 3: test=(uint8_t)data;adder=0;break;
            }
            return true;
        }
        if (addr>=0x8000U && addr<=0x87ffU) {prg[addr&3U]=(uint8_t)data;return true;}
        if (addr>=0x9000U && addr<=0x97ffU) {
            unsigned i=addr&7U;chr[i]=(uint16_t)((chr[i]&0xff00U)|data);return true;
        }
        if (addr>=0xa000U && addr<=0xa7ffU) {
            unsigned i=addr&7U;chr[i]=(uint16_t)((chr[i]&0x00ffU)|(data<<8));return true;
        }
        if (addr>=0xb000U && addr<=0xb7ffU) {
            unsigned i=addr&3U;
            if (addr&4U) nt[i]=(uint16_t)((nt[i]&255U)|(data<<8));
            else nt[i]=(uint16_t)((nt[i]&0xff00U)|data);
            return true;
        }
        if (addr>=0xc000U && addr<=0xcfffU) {write_irq(addr,data);return true;}
        if (addr>=0xd000U && addr<=0xd7ffU) {write_mode(addr,data);return true;}
        return false;
    }
    // DIP readback at 5000/5400/5C00 is intentionally not implemented.
    bool read_alu(unsigned addr,unsigned &value) const {
        if (addr<0x5800U || addr>=0x6000U) return false;
        switch (addr&3U) {
        case 0:value=(unsigned(mul[0])*unsigned(mul[1]))&255U;break;
        case 1:value=(unsigned(mul[0])*unsigned(mul[1]))>>8;break;
        case 2:value=adder;break;
        default:value=test;break;
        }
        return true;
    }
    // Exact edges must come from the PPU/CPU scheduler, not synthesize eight
    // A12 pulses per scanline or reuse $2006/$2007 notifications.
    void on_cpu_cycles(unsigned clocks) {
        if ((irq_control&3U)==0U) for (unsigned i=0;i<clocks;++i) irq_clock();
    }
    // vN68: exact O(1) CPU-clock state evolution. Unlike IRQ prediction,
    // accounting MUST continue after the IRQ becomes pending. Differential
    // tests compare this against single-clock stepping including wraparound.
    void on_cpu_cycles_fast(unsigned clocks) {
        if ((irq_control & 3U)!=0U || !irq_enable || !clocks) return;
        const unsigned dir=irq_control & 0xc0U;
        if (dir!=0x40U && dir!=0x80U) return;
        const unsigned mask=(irq_control & 4U)?7U:255U;
        const unsigned period=mask+1U;
        const unsigned pres=unsigned(irq_prescaler)&mask;
        const unsigned first=(dir==0x40U)?(period-pres):(pres?pres:period);
        unsigned long ticks=0;
        if(clocks>=first) ticks=1UL+(unsigned long)(clocks-first)/period;
        const unsigned low=(dir==0x40U)?((pres+clocks)&mask):((pres-clocks)&mask);
        irq_prescaler=(uint8_t)((unsigned(irq_prescaler)&~mask)|low);
        if(!ticks)return;
        const unsigned count=irq_counter;
        const bool freeze=(irq_control&8U)!=0;
        if(freeze) {
            if((dir==0x40U && count==0U)||(dir==0x80U && count==255U))irq_pending=1;
            return;
        }
        const unsigned until_irq=(dir==0x40U)?(count?(256U-count):256U):(count+1U);
        if(ticks>=until_irq)irq_pending=1;
        irq_counter=(uint8_t)((dir==0x40U)?((count+ticks)&255UL):((count-ticks)&255UL));
    }
    void on_ppu_a12_rise() {
        if ((irq_control&3U)==1U) irq_clock();
    }
    void on_ppu_address(unsigned address) {
        address&=0x3fffU;
        if ((irq_control&3U)==2U && address!=last_ppu_addr) {
            irq_clock();irq_clock(); // upstream two clocks on address change
        }
        if ((mode[3]&0x80U) && (mode[0]&0x18U)==0x08U) {
            const unsigned a=address&0x2ff0U;
            if (a==0x0fd0U || a==0x0fe0U) {
                const unsigned b=(address>>12)&1U;
                const unsigned chrsel=(address>>4)&(((address>>10)&4U)|2U);
                latch[b]=(uint8_t)chrsel;
            }
        }
        last_ppu_addr=(uint16_t)address;
    }
    bool pending_irq() const {return irq_pending!=0;}
    unsigned irq_source() const {return irq_control&3U;}
    bool irq_is_enabled() const {return irq_enable!=0;}
    // CPU clock predictor: analytical, with no forward mutation. CPU source
    // counter crosses only on prescaler rollover; returns clocks to assertion.
    // 0 denotes pending now; ~0U means no CPU-driven IRQ predicted.
    unsigned cpu_clocks_until_irq() const {
        if (irq_pending) return 0U;
        if (!irq_enable || (irq_control&3U)!=0U) return ~0U;
        const unsigned direction=irq_control&0xc0U;
        if (direction!=0x40U && direction!=0x80U) return ~0U;
        const unsigned mask=(irq_control&4U)?7U:255U;
        const unsigned p=irq_prescaler&mask;
        const unsigned first=(direction==0x40U)?(mask+1U-p):(p?p:mask+1U);
        if ((irq_control&8U) && ((direction==0x40U && irq_counter!=0U) ||
                                 (direction==0x80U && irq_counter!=255U))) return ~0U;
        const unsigned ticks=(irq_control&8U)?1U:
                       (direction==0x40U ? 256U-irq_counter : irq_counter+1U);
        return first + (ticks-1U)*(mask+1U);
    }
    void map(Mapping &out) const {
        const unsigned last=(mode[0]&4U)?prg[3]:255U;
        unsigned p[4]={0,0,0,0};
        switch (mode[0]&3U) {
        case 0:for (unsigned i=0;i<4U;++i)p[i]=(last<<2)|i;break;
        case 1:p[0]=(unsigned(prg[1])<<1);p[1]=p[0]|1U;
               p[2]=(last<<1);p[3]=p[2]|1U;break;
        case 2:p[0]=prg[0];p[1]=prg[1];p[2]=prg[2];p[3]=last;break;
        default:p[0]=reverse_7(prg[0]);p[1]=reverse_7(prg[1]);
                p[2]=reverse_7(prg[2]);p[3]=reverse_7(last);break;
        }
        for(unsigned i=0;i<4U;++i)out.prg8[i]=transform_prg(p[i]);
        for(unsigned i=0;i<8U;++i) {
            unsigned v=0;
            switch(mode[0]&0x18U) {
            case 0x00U:v=(unsigned(chr[0])<<3)|i;break;
            case 0x08U: {
                const unsigned side=i>>2;
                const unsigned index=(unsigned(latch[side])&2U)|(side?4U:0U);
                v=(unsigned(chr[index])<<2)|(i&3U);break;
            }
            case 0x10U:v=(unsigned(chr[i&~1U])<<1)|(i&1U);break;
            default:v=chr[i];break;
            }
            out.chr1[i]=transform_chr(v);
        }
        unsigned w=prg[3];
        switch(mode[0]&3U) {
        case 0:w=(w<<2)|3U;break;
        case 1:w=(w<<1)|1U;break;
        case 3:w=reverse_7(w);break;
        }
        out.wram_rom_bank=transform_prg(w);
        out.wram_is_rom=(mode[0]&0x80U)!=0;
        out.chr_write_protected=(mode[2]&0x40U)!=0;
        for(unsigned i=0;i<4U;++i) {
            if (mode[0]&0x20U) {
                if (((nt[i]^mode[2])&0x80U)||(mode[0]&0x40U)) {
                    out.nt_source[i]=CHR_ROM;out.nt_bank[i]=transform_chr(nt[i]);
                } else {out.nt_source[i]=CIRAM;out.nt_bank[i]=nt[i]&1U;}
            } else if (mode[1]&8U) {
                out.nt_source[i]=CIRAM;out.nt_bank[i]=nt[i]&1U;
            } else {
                out.nt_source[i]=CIRAM;
                const unsigned m=mode[1]&3U;
                out.nt_bank[i]=m==0U?(i&1U):m==1U?(i>>1):m==2U?0U:1U;
            }
        }
    }
    // Byte-exact state ABI, no compiler struct padding; ROM/RAM not included.
    void save(uint8_t out[snapshot_size]) const {
        unsigned k=0;
        for(unsigned i=0;i<4U;++i)out[k++]=mode[i];
        for(unsigned i=0;i<4U;++i)out[k++]=prg[i];
        for(unsigned i=0;i<2U;++i)out[k++]=mul[i];
        out[k++]=adder;out[k++]=test;
        for(unsigned i=0;i<2U;++i)out[k++]=latch[i];
        for(unsigned i=0;i<8U;++i){out[k++]=(uint8_t)chr[i];out[k++]=(uint8_t)(chr[i]>>8);}
        for(unsigned i=0;i<4U;++i){out[k++]=(uint8_t)nt[i];out[k++]=(uint8_t)(nt[i]>>8);}
        out[k++]=irq_control;out[k++]=irq_enable;
        out[k++]=irq_prescaler;out[k++]=irq_counter;out[k++]=irq_xor;
        out[k++]=irq_pending;
        out[k++]=(uint8_t)last_ppu_addr;out[k++]=(uint8_t)(last_ppu_addr>>8);
    }
    void load(const uint8_t in[snapshot_size]) {
        unsigned k=0;
        for(unsigned i=0;i<4U;++i)mode[i]=in[k++];
        for(unsigned i=0;i<4U;++i)prg[i]=in[k++];
        for(unsigned i=0;i<2U;++i)mul[i]=in[k++];
        adder=in[k++];test=in[k++];
        for(unsigned i=0;i<2U;++i)latch[i]=in[k++];
        for(unsigned i=0;i<8U;++i){chr[i]=in[k++];chr[i]|=(uint16_t)(in[k++]<<8);}
        for(unsigned i=0;i<4U;++i){nt[i]=in[k++];nt[i]|=(uint16_t)(in[k++]<<8);}
        irq_control=in[k++];irq_enable=in[k++];irq_prescaler=in[k++];irq_counter=in[k++];
        irq_xor=in[k++];irq_pending=in[k++];last_ppu_addr=in[k++];
        last_ppu_addr|=(uint16_t)(in[k++]<<8);
    }
};
