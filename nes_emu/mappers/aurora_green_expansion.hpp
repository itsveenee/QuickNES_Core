#pragma once
/* AURORA_QN_11_MAPPERS_REVIEW_V2_20261017
   Aurora QuickNES mapper expansion. Derived from hardware register semantics
   documented in Mesen2/Nestopia, adapted to QuickNES scheduling API.
   Do not advertise any mapper not implemented here. */
#include "Nes_Mapper.h"
#include "mapper004.hpp"
#include <stdint.h>
#include <string.h>

/* QuickNES approximates PPU A12 fetch activity at scanline granularity.
   Use the same scheduler for A12-counting boards, keeping CPU $2006/$2007
   notifications coherent without requiring a costly PPU rewrite. */
class AuroraA12Mapper : public Nes_Mapper {
protected:
    enum {
        first_a12 = 20 * Nes_Ppu::scanline_len + 268,
        last_a12  = first_a12 + 240 * Nes_Ppu::scanline_len
    };
    int32_t a12_next;
    bool a12_active() const {
        if (!(emu().ppu.w2001 & 0x18)) return false;
        const int ctrl = emu().ppu.w2000;
        return (ctrl & 0x20) || ((ctrl & 0x18) == 0x08) || ((ctrl & 0x18) == 0x10);
    }
    virtual void a12_tick() = 0;
    void run_a12(nes_time_t t) {
        const int32_t to = (int32_t)(t * ppu_overclock);
        const bool active = a12_active();
        if (a12_next < 0) a12_next = first_a12;
        while (a12_next < to && a12_next <= last_a12) {
            if (active) a12_tick();
            a12_next += Nes_Ppu::scanline_len;
        }
    }
    void irq_a12_end_frame(nes_time_t t) {
        run_a12(t);
        a12_next = first_a12;
    }
    nes_time_t a12_due(nes_time_t present, int clocks) {
        run_a12(present);
        if (clocks <= 0 || !a12_active()) return no_irq;
        const int32_t ppu = a12_next + (clocks - 1) * Nes_Ppu::scanline_len;
        return ppu > last_a12 ? no_irq : (nes_time_t)(ppu / ppu_overclock + 2);
    }
public:
    AuroraA12Mapper() : a12_next(first_a12) { }
    bool ppu_ctrl_mask_irq_sensitive() const { return true; }
    void a12_clocked() { a12_tick(); irq_changed(); }
};

// BTL-DRAGONNINJA: mapper 222 (Dragon Ninja, SMB8 bootleg).
class Mapper222 : public AuroraA12Mapper {
    struct state_t {
        uint8_t prg[2], chr[8], mirror, irq_counter, irq_pending, reserved;
        int32_t a12_cursor;
    } s;
    void a12_tick() {
        if (s.irq_counter && (unsigned)s.irq_counter + 1 >= 240) {
            s.irq_counter = 0;
            s.irq_pending = 1;
        } else if (s.irq_counter) {
            ++s.irq_counter;
        }
    }
    void apply_banks() {
        set_prg_bank(0x8000, bank_8k, s.prg[0]);
        set_prg_bank(0xa000, bank_8k, s.prg[1]);
        set_prg_bank(0xc000, bank_8k, last_bank - 1);
        set_prg_bank(0xe000, bank_8k, last_bank);
        for (int i = 0; i < 8; ++i) set_chr_bank(i * 0x400, bank_1k, s.chr[i]);
        if (s.mirror & 1) mirror_horiz(); else mirror_vert();
    }
public:
    Mapper222() { register_state(&s, sizeof s); }
    void reset_state() {
        memset(&s, 0, sizeof s);
        s.prg[0] = 0; s.prg[1] = 1;
        for (int i=0; i<8; ++i) s.chr[i] = (uint8_t)i;
        a12_next = first_a12;
    }
    void apply_mapping() {
        a12_next = s.a12_cursor ? s.a12_cursor : first_a12;
        apply_banks();
    }
    void run_until(nes_time_t t) {
        run_a12(t); s.a12_cursor = a12_next;
    }
    void end_frame(nes_time_t t) {
        irq_a12_end_frame(t); s.a12_cursor = a12_next;
    }
    nes_time_t next_irq(nes_time_t t) {
        run_until(t);
        if (s.irq_pending) return t;
        return s.irq_counter ? a12_due(t, 240 - s.irq_counter) : no_irq;
    }
    void write(nes_time_t t, nes_addr_t a, int v) {
        run_until(t);
        switch (a & 0xf003) {
        case 0x8000: s.prg[0] = (uint8_t)v; set_prg_bank(0x8000,bank_8k,v); break;
        case 0xa000: s.prg[1] = (uint8_t)v; set_prg_bank(0xa000,bank_8k,v); break;
        case 0x9000: s.mirror = (uint8_t)v; if(v&1) mirror_horiz(); else mirror_vert(); break;
        case 0xb000: case 0xb002: case 0xc000: case 0xc002:
        case 0xd000: case 0xd002: case 0xe000: case 0xe002: {
            const int n = (((a >> 12) - 0xb) << 1) | ((a >> 1) & 1);
            s.chr[n] = (uint8_t)v; set_chr_bank(n * 0x400,bank_1k,v); break;
        }
        case 0xf000:
            s.irq_counter = (uint8_t)v; s.irq_pending = 0;
            irq_changed(); break;
        }
    }
};

// Mapper 117: decreasing A12 IRQ, independent PRG and CHR bank registers.
class Mapper117 : public AuroraA12Mapper {
    struct state_t {
        uint8_t prg[4], chr[8], mirror, counter, latch;
        uint8_t enable, armed, pending;
        int32_t a12_cursor;
        uint8_t prg_written_mask;
    } s;
    void a12_tick() {
        if (s.enable && s.armed && s.counter) {
            if (--s.counter == 0) { s.armed = 0; s.pending = 1; }
        }
    }
public:
    Mapper117() { register_state(&s, sizeof s); }
    void reset_state() {
        memset(&s,0,sizeof s);
        for(int i=0;i<4;++i) s.prg[i]=(uint8_t)(0xfc+i);
        for(int i=0;i<8;++i) s.chr[i]=(uint8_t)i;
        a12_next=first_a12;
    }
    void apply_mapping() {
        a12_next = s.a12_cursor ? s.a12_cursor : first_a12;
        for(int i=0;i<4;++i) {
            const int bank = ((s.prg_written_mask & (1 << i)) ||
                              s.prg[i] != (uint8_t)(0xfc+i)) ? s.prg[i] : (i-4);
            set_prg_bank(0x8000+i*0x2000,bank_8k,bank);
        }
        for(int i=0;i<8;++i) set_chr_bank(i*0x400,bank_1k,s.chr[i]);
        if(s.mirror&1) mirror_horiz(); else mirror_vert();
    }
    void run_until(nes_time_t t) { run_a12(t); s.a12_cursor = a12_next; }
    void end_frame(nes_time_t t) { irq_a12_end_frame(t); s.a12_cursor = a12_next; }
    nes_time_t next_irq(nes_time_t t) {
        run_until(t);
        if(s.pending) return t;
        return s.enable && s.armed && s.counter ? a12_due(t,s.counter) : no_irq;
    }
    void write(nes_time_t t,nes_addr_t a,int v) {
        run_until(t);
        if(a>=0x8000 && a<=0x8003) {
            const int i=a&3; s.prg[i]=(uint8_t)v; s.prg_written_mask |= (uint8_t)(1 << i);
            set_prg_bank(0x8000+i*0x2000,bank_8k,v);
        } else if(a>=0xa000 && a<=0xa007) {
            const int i=a&7; s.chr[i]=(uint8_t)v;
            set_chr_bank(i*0x400,bank_1k,v);
        } else switch(a) {
        case 0xc001: s.latch=(uint8_t)v; break;
        case 0xc002: s.pending=0; irq_changed(); break;
        case 0xc003: s.counter=s.latch; s.armed=1; irq_changed(); break;
        case 0xd000: s.mirror=(uint8_t)v; if(v&1)mirror_horiz();else mirror_vert();break;
        case 0xe000: s.enable=(uint8_t)(v&1);s.pending=0;irq_changed();break;
        }
    }
};

// Cony/Yoko mapper 83: dynamic PRG mode, 1 KiB/2 KiB CHR, CPU IRQ.
class Mapper083 : public Nes_Mapper {
    struct state_t {
        uint8_t reg[11], ext[4], mode, bank, chr2k, chr1k;
        uint8_t irq_on, irq_pending;
        uint16_t irq_counter;
        int32_t last_time;
    } s;
    void map_banks() {
        switch(s.mode&3) {
        case 0: mirror_vert(); break;
        case 1: mirror_horiz(); break;
        case 2: mirror_single(0);break;
        default:mirror_single(1);break;
        }
        if(s.chr2k && !s.chr1k) {
            set_chr_bank(0x0000,bank_2k,s.reg[0]);
            set_chr_bank(0x0800,bank_2k,s.reg[1]);
            set_chr_bank(0x1000,bank_2k,s.reg[6]);
            set_chr_bank(0x1800,bank_2k,s.reg[7]);
        } else for(int i=0;i<8;++i)
            set_chr_bank(i*0x400,bank_1k,s.reg[i]|((s.bank&0x30)<<4));
        if(s.mode&0x40) {
            set_prg_bank(0x8000,bank_16k,s.bank&0x3f);
            set_prg_bank(0xc000,bank_16k,(s.bank&0x30)|0x0f);
        } else {
            for(int i=0;i<3;++i) set_prg_bank(0x8000+i*0x2000,bank_8k,s.reg[i+8]);
            set_prg_bank(0xe000,bank_8k,last_bank);
        }
    }
public:
    Mapper083(){register_state(&s,sizeof s);}
    void reset_state(){memset(&s,0,sizeof s);}
    void apply_mapping(){
        intercept_writes(0x5000,0x3000);
        intercept_reads(0x5000,0x1000);
        map_banks();
    }
    void run_until(nes_time_t t) {
        if(t<=s.last_time) return;
        if(s.irq_on && !s.irq_pending) {
            unsigned elapsed=(unsigned)(t-s.last_time);
            unsigned remain=s.irq_counter?s.irq_counter:65536u;
            if(elapsed>=remain) {s.irq_on=0;s.irq_counter=0xffff;s.irq_pending=1;}
            else s.irq_counter=(uint16_t)(s.irq_counter-elapsed);
        }
        s.last_time=(int32_t)t;
    }
    void end_frame(nes_time_t t){run_until(t);s.last_time-=(int32_t)t;}
    nes_time_t next_irq(nes_time_t t){
        run_until(t);
        if(s.irq_pending)return t;
        if(!s.irq_on)return no_irq;
        return (nes_time_t)s.last_time+(s.irq_counter?s.irq_counter:65536);
    }
    int read(nes_time_t,nes_addr_t a){
        if (a == 0x5000) return 0; /* DIP switches = 0, unknown high open-bus bits */
        if(a>=0x5100 && a<=0x5103)return s.ext[a&3];
        return -1;
    }
    bool write_intercepted(nes_time_t,nes_addr_t a,int v) {
        if(a>=0x5100 && a<=0x5103){s.ext[a&3]=(uint8_t)v;return true;}
        return false;
    }
    void write(nes_time_t t,nes_addr_t a,int v){
        run_until(t);
        if(a>=0x8300 && a<=0x8302){s.mode&=0xbf;s.reg[(a-0x8300)+8]=(uint8_t)v;map_banks();}
        else if(a>=0x8310 && a<=0x8317){s.reg[a-0x8310]=(uint8_t)v;
            if(a>=0x8312 && a<=0x8315) s.chr1k=1;
            map_banks();}
        else switch(a){
        case 0x8000:s.chr2k=1;s.bank=(uint8_t)v;s.mode|=0x40;map_banks();break;
        case 0xb000:case 0xb0ff:case 0xb1ff:s.bank=(uint8_t)v;s.mode|=0x40;map_banks();break;
        case 0x8100:s.mode=(uint8_t)(v|(s.mode&0x40));map_banks();break;
        case 0x8200:s.irq_counter=(uint16_t)((s.irq_counter&0xff00)|(v&0xff));s.irq_pending=0;irq_changed();break;
        case 0x8201:s.irq_on=(uint8_t)((s.mode&0x80)!=0);
            s.irq_counter=(uint16_t)((s.irq_counter&0xff)|((v&0xff)<<8));irq_changed();break;
        }
    }
};

// Mapper 116: Huang-1 hybrid VRC2 / MMC3 / MMC1.
class Mapper116 : public AuroraA12Mapper {
    struct state_t {
        uint8_t mode, vchr[8], vprg[2], vmirror;
        uint8_t mregs[10], mctrl, mmirror;
        uint8_t one[4], one_buf, one_shift;
        uint8_t irq_ctr, irq_latch, irq_reload, irq_on, irq_pending;
        int32_t a12_cursor;
        uint8_t mregs_written_mask;
    } s;
    void a12_tick(){
        if((s.mode&3)!=1)return;
        if(s.irq_ctr==0||s.irq_reload)s.irq_ctr=s.irq_latch;
        else --s.irq_ctr;
        s.irq_reload=0;
        if(s.irq_ctr==0 && s.irq_on)s.irq_pending=1;
    }
    void banks(){
        const int which=s.mode&3;
        if(which==0) {
            set_prg_bank(0x8000,bank_8k,s.vprg[0]);
            set_prg_bank(0xa000,bank_8k,s.vprg[1]);
            set_prg_bank(0xc000,bank_8k,last_bank-1);
            set_prg_bank(0xe000,bank_8k,last_bank);
            for(int i=0;i<8;++i)
                set_chr_bank(i*0x400,bank_1k,((s.mode&4)<<6)|s.vchr[i]);
            if(s.vmirror&1)mirror_horiz();else mirror_vert();
        }else if(which==1){
            const int pm=(s.mctrl>>5)&2;
            const int pages[4] = {6 + pm, 7, 6 + (pm ^ 2), 9};
            for(int i=0; i<4; ++i) {
                const int reg = pages[i];
                const bool fixed = reg >= 8;
                const bool user_written = ((s.mregs_written_mask & (1u << reg)) != 0) ||
                                          (reg < 8 && s.mregs[reg] != (uint8_t)(0xfc+reg-6));
                const int bank = (fixed || !user_written) ? (int8_t)s.mregs[reg] : s.mregs[reg];
                set_prg_bank(0x8000+i*0x2000,bank_8k,bank);
            }
            const int outer=(s.mode&4)<<6;
            int ch[8], x=(s.mctrl&0x80)?4:0;
            ch[0^x]=(s.mregs[0]&0xfe)|outer; ch[1^x]=(s.mregs[0]|1)|outer;
            ch[2^x]=(s.mregs[1]&0xfe)|outer; ch[3^x]=(s.mregs[1]|1)|outer;
            for(int i=0;i<4;++i)ch[(i+4)^x]=s.mregs[i+2]|outer;
            for(int i=0;i<8;++i)set_chr_bank(i*0x400,bank_1k,ch[i]);
            if(s.mmirror&1)mirror_horiz();else mirror_vert();
        } else {
            int bank=s.one[3]&0x0f;
            if(s.one[0]&8){
                if(s.one[0]&4){set_prg_bank(0x8000,bank_16k,bank);set_prg_bank(0xc000,bank_16k,15);}
                else{set_prg_bank(0x8000,bank_16k,0);set_prg_bank(0xc000,bank_16k,bank);}
            } else set_prg_bank(0x8000,bank_32k,(bank&0xfe)>>1);
            if(s.one[0]&0x10){
                set_chr_bank(0x0000,bank_4k,s.one[1]);
                set_chr_bank(0x1000,bank_4k,s.one[2]);
            }else set_chr_bank(0x0000,bank_8k,(s.one[1]&0xfe)>>1);
            switch(s.one[0]&3){
            case 0:mirror_single(0);break;case 1:mirror_single(1);break;
            case 2:mirror_vert();break;case 3:mirror_horiz();break;
            }
        }
    }
public:
    Mapper116(){register_state(&s,sizeof s);}
    void reset_state(){
        memset(&s,0,sizeof s);for(int i=0;i<4;++i)s.vchr[i]=0xff;
        for(int i=4;i<8;++i)s.vchr[i]=(uint8_t)i;
        s.vprg[0]=0;s.vprg[1]=1;
        const uint8_t def[]={0,2,4,5,6,7,0xfc,0xfd,0xfe,0xff};
        memcpy(s.mregs,def,sizeof def);s.one[0]=12;a12_next=first_a12;
    }
    void apply_mapping(){
        a12_next=s.a12_cursor?s.a12_cursor:first_a12;
        intercept_writes(0x4100,0x3f00);
        banks();
    }
    bool write_intercepted(nes_time_t t,nes_addr_t a,int v){
        if(a<0x8000 && (a&0x4100)==0x4100){
            run_until(t);s.mode=(uint8_t)v;
            if(a&1){s.one[0]=12;s.one[3]=0;s.one_buf=s.one_shift=0;}
            banks();irq_changed();return true;
        }
        return false;
    }
    void run_until(nes_time_t t){run_a12(t);s.a12_cursor=a12_next;}
    void end_frame(nes_time_t t){irq_a12_end_frame(t);s.a12_cursor=a12_next;}
    nes_time_t next_irq(nes_time_t t){
        run_until(t);
        if(s.irq_pending)return t;
        if((s.mode&3)!=1||!s.irq_on)return no_irq;
        uint8_t c=s.irq_ctr;bool reload=s.irq_reload!=0;
        for(int n=1;n<=257;++n){
            if(c==0||reload)c=s.irq_latch;
            else --c;
            reload=false;
            if(c==0)return a12_due(t,n);
        }
        return no_irq;
    }
    void write(nes_time_t t,nes_addr_t a,int v){
        run_until(t);
        if((s.mode&3)==0){
            if(a>=0xb000 && a<=0xe003){
                int i=(( ((a&2)|(a>>10))>>1)+2)&7;
                int shift=(a&1)*4;
                s.vchr[i]=(uint8_t)((s.vchr[i]&(0xf0>>shift))|((v&15)<<shift));
            } else switch(a&0xf000){
                case 0x8000:s.vprg[0]=(uint8_t)v;break;
                case 0xa000:s.vprg[1]=(uint8_t)v;break;
                case 0x9000:s.vmirror=(uint8_t)v;break;
            }
        }else if((s.mode&3)==1){
            switch(a&0xe001){
            case 0x8000:s.mctrl=(uint8_t)v;break;
            case 0x8001:s.mregs[s.mctrl&7]=(uint8_t)v;s.mregs_written_mask |= (uint8_t)(1u << (s.mctrl&7));break;
            case 0xa000:s.mmirror=(uint8_t)v;break;
            case 0xc000:s.irq_latch=(uint8_t)v;break;
            case 0xc001:s.irq_reload=1;break;
            case 0xe000:s.irq_on=0;s.irq_pending=0;break;
            case 0xe001:s.irq_on=1;break;
            }
        }else {
            if(v&0x80){s.one[0]|=12;s.one_buf=s.one_shift=0;}
            else{
                int idx=(a>>13)-4;
                s.one_buf|=(uint8_t)((v&1)<<s.one_shift++);
                if(s.one_shift==5){s.one[idx]=s.one_buf;
                    s.one_buf=s.one_shift=0;}
            }
        }
        banks();irq_changed();
    }
};

/* MMC3 derivative bank/command quirks. Mapper004 retains native IRQ scheduler. */
template<int Id> class AuroraMmc3Variant : public Mapper004 {
    struct ext_state_t { uint8_t r[8]; } e;
    static const uint8_t* security() {
        static const uint8_t perm[8] = {0,3,1,5,6,7,2,4}; return perm;
    }
    void low(nes_time_t t, nes_addr_t a, int v) {
        (void)t;
        if (Id == 189) { if (a >= 0x4120) { e.r[0]=(uint8_t)v; update_prg_banks(); } }
        if (Id == 114) { if (a >= 0x5000) { e.r[0]=(uint8_t)v; update_prg_banks(); } }
        if (Id == 115) {
            if (a == 0x5080) e.r[2]=(uint8_t)v;
            else if (a >= 0x4100) {
                if (a&1) e.r[1]=(uint8_t)(v&1);
                else e.r[0]=(uint8_t)v;
                update_prg_banks(); update_chr_banks();
            }
        }
        if (Id == 121 && a >= 0x5000 && a <= 0x5fff) {
            static const uint8_t lut[4]={0x83,0x83,0x42,0};
            e.r[4] = lut[v&3];
            if ((a&0x5180)==0x5180) { e.r[3]=(uint8_t)v; update_prg_banks(); update_chr_banks(); }
        }
        if (Id == 123 && a >= 0x5800 && a <= 0x5fff) {
            e.r[a&1]=(uint8_t)v; update_prg_banks();
        }
        if (Id == 215) {
            if (a==0x5000) {e.r[0]=(uint8_t)v; update_prg_banks();update_chr_banks();}
            if (a==0x5001) {e.r[1]=(uint8_t)v; update_prg_banks();update_chr_banks();}
            if (a==0x5007) e.r[2]=(uint8_t)(v&7);
        }
    }
    void prg_page(int slot, int bank) {
        if (Id == 121) {
            int outer=(e.r[3]&0x80)>>2;
            set_prg_bank(0x8000+slot*0x2000,bank_8k,(bank&31)|outer);
        } else if(Id == 215) {
            const int mask=(e.r[0]&0x40)?15:31;
            const int high=((e.r[1]&3)<<5)|((e.r[0]&0x40)?(e.r[1]&0x10):0);
            set_prg_bank(0x8000+slot*0x2000,bank_8k,(bank&mask)|high);
        } else set_prg_bank(0x8000+slot*0x2000,bank_8k,bank);
    }
public:
    AuroraMmc3Variant() : Mapper004() { memset(&e,0,sizeof e); }
    void reset_state() {
        Mapper004::reset_state(); memset(&e,0,sizeof e);
        if(Id==121) e.r[3]=0x80;
        if(Id==215) e.r[1]=3;
    }
    void apply_mapping() {
        /* Never dispatch the base reset's synthetic $A000/$A001 writes into
           a bootleg's translated register map. Preserve MMC3 RAM semantics. */
        if (!(cart().mirroring()&8)) {
            if(mirror&1) mirror_horiz(); else mirror_vert();
        }
        if ((sram_mode & 0x3f) == 0x30) enable_sram(true);
        else enable_sram((sram_mode&0x80)!=0,(sram_mode&0x40)!=0);
        update_chr_banks(); update_prg_banks(); start_frame();
        if(Id==189 || Id==114 || Id==115 || Id==121 || Id==123 || Id==215)
            intercept_writes(0x4100,0x3f00);
        if(Id==115 || Id==121) intercept_reads(0x5000,0x1000);
    }
    void save_state(mapper_state_t& out) {
        Mapper004::save_state(out);
        if(out.size+sizeof e<=max_mapper_state_size) {
            memcpy(out.data+out.size,&e,sizeof e);
            out.size+=(int)sizeof e;
        }
    }
    void load_state(mapper_state_t const& in) {
        /* Mapper004 state is the first 15 bytes, extensions follow it.
           Legacy MMC3 snapshots remain loadable with zeroed extension. */
        const unsigned old_size=sizeof(mmc3_state_t);
        mapper_state_t base;
        base.size=(in.size < (int)old_size) ? in.size : (int)old_size;
        if(base.size<0) base.size=0;
        memcpy(base.data,in.data,base.size);
        memset(&e,0,sizeof e);
        if(Id==121) e.r[3]=0x80;
        if(Id==215) e.r[1]=3;
        if(in.size >= (int)(old_size+sizeof e)) memcpy(&e,in.data+old_size,sizeof e);
        Mapper004::load_state(base);
    }
    int read(nes_time_t,nes_addr_t a) {
        if(Id==115 && a>=0x5000 && a<0x6000) return e.r[2];
        if(Id==121 && a>=0x5000 && a<0x6000) return e.r[4];
        return -1;
    }
    bool write_intercepted(nes_time_t t,nes_addr_t a,int v) {
        if (a < 0x8000) {
            if((Id==189 && a>=0x4120) || (Id==114 && a>=0x5000) ||
               (Id==115 && a>=0x4100) || (Id==121 && a>=0x5000) ||
               (Id==123 && a>=0x5800) || (Id==215 && a>=0x5000)) {
                low(t,a,v); return true;
            }
        }
        return false;
    }
    void update_prg_banks() {
        if(Id==189) {
            const int bank=((e.r[0]|(e.r[0]>>4))&7)*4;
            for(int i=0;i<4;++i) prg_page(i,bank+i);
        } else if(Id==114 && (e.r[0]&0x80)) {
            const int b=(e.r[0]&15)*2;
            prg_page(0,b);prg_page(1,b+1);prg_page(2,b);prg_page(3,b+1);
        } else if(Id==115 && (e.r[0]&0x80)) {
            const int b=(e.r[0]&0x20)?((e.r[0]&15)>>1)*4:(e.r[0]&15)*2;
            for(int i=0;i<4;++i)prg_page(i,b+((e.r[0]&0x20)?i:(i&1)));
        } else if(Id==123 && (e.r[0]&0x40)) {
            const int b=(e.r[0]&5)|((e.r[0]&8)>>2)|((e.r[0]&0x20)>>2);
            const int start=(e.r[0]&2)?((b&~1)*2):(b*2);
            for(int i=0;i<4;++i)prg_page(i,start+((e.r[0]&2)?i:(i&1)));
        } else if(Id==215 && (e.r[0]&0x80)) {
            const int bank=((e.r[1]&3)<<4)|((e.r[0]&0x40)?((e.r[0]&7)|((e.r[1]&0x10)>>1)):(e.r[0]&15));
            for(int i=0;i<4;++i) set_prg_bank(0x8000+i*0x2000,bank_8k,bank*2+((e.r[0]&0x20)?i:(i&1)));
        } else {
            const int slot=(mode>>6)&1;
            const int slot0=slot?last_bank-1:banks[6];
            const int slot2=slot?banks[6]:last_bank-1;
            prg_page(0,slot0);prg_page(1,banks[7]);prg_page(2,slot2);prg_page(3,last_bank);
            if(Id==121 && (e.r[5]&63)) {
                const int outer=(e.r[3]&0x80)>>2;
                for(int i=1;i<=3;++i)
                    set_prg_bank(0x8000+i*0x2000,bank_8k,e.r[3-i]|outer);
            }
        }
    }
    void update_chr_banks() {
        if(Id==197) {
            if(!(mode&0x80)) {
                set_chr_bank(0x0000,bank_4k,banks[0] >> 1);
                set_chr_bank(0x1000,bank_2k,banks[2]);
                set_chr_bank(0x1800,bank_2k,banks[3]);
            } else {
                set_chr_bank(0x0000,bank_4k,banks[2] >> 1);
                set_chr_bank(0x1000,bank_2k,banks[0]);
                set_chr_bank(0x1800,bank_2k,banks[0]);
            }
            return;
        }
        const int x=(mode&0x80)?4:0;
        int p[8]={banks[0]&~1,banks[0]|1,banks[1]&~1,banks[1]|1,
                   banks[2],banks[3],banks[4],banks[5]};
        for(int i=0;i<8;++i) {
            const int slot=i^x;
            int b=p[i];
            if(Id==115) b|=e.r[1]<<8;
            if(Id==121){
                if(cart().prg_size()==cart().chr_size()) b|=(e.r[3]&0x80)<<1;
                else if ((slot<4 && !x)||(slot>=4 && x)) b|=0x100;
            }
            if(Id==215) {
                const int hi=((e.r[1]&12)<<6)|((e.r[1]&0x20)<<2);
                b=(e.r[0]&0x40)?(hi|(b&0x7f)):((e.r[1]&12)<<6)|b;
            }
            set_chr_bank(slot*0x400,bank_1k,b);
        }
    }
    /* MMC3A (mapper 114) suppresses a counter==0 IRQ unless it was
       decremented from nonzero or came from an explicit reload. */
    void clock_114() {
        const uint8_t before = irq_ctr;
        const bool reload = e.r[6] != 0;
        if (counter_just_clocked) --counter_just_clocked;
        if (reload) { irq_ctr = irq_latch; }
        else if (!irq_ctr--) irq_ctr = irq_latch;
        if (irq_ctr == 0 && (before != 0 || reload)) irq_flag = irq_enabled;
        e.r[6] = 0;
    }
    void run_until(nes_time_t t) {
        if (Id != 114) { Mapper004::run_until(t); return; }
        const bool active = scanline_a12_clocking();
        if (next_time < 0) next_time = 0;
        const int32_t stop = (int32_t)(t * ppu_overclock);
        while (next_time < stop && next_time <= last_scanline) {
            if (active) clock_114();
            next_time += Nes_Ppu::scanline_len;
        }
    }
    void a12_clocked() {
        if (Id != 114) { Mapper004::a12_clocked(); return; }
        clock_114();
        if (irq_enabled) irq_changed();
    }
    void end_frame(nes_time_t t) {
        if (Id != 114) { Mapper004::end_frame(t); return; }
        run_until(t);
        start_frame();
    }
    nes_time_t next_irq(nes_time_t t) {
        if (Id != 114) return Mapper004::next_irq(t);
        run_until(t);
        if (!irq_enabled) return no_irq;
        if (irq_flag) return t;
        if (!scanline_a12_clocking()) return no_irq;
        uint8_t c = irq_ctr;
        bool reload = e.r[6] != 0;
        int32_t cursor = next_time;
        while (cursor <= last_scanline) {
            const uint8_t before = c;
            if (reload) c = irq_latch;
            else if (!c--) c = irq_latch;
            if (c == 0 && (before != 0 || reload))
                return cursor / ppu_overclock + 1;
            reload = false;
            cursor += Nes_Ppu::scanline_len;
        }
        return no_irq;
    }
    void base_write(nes_time_t t, nes_addr_t a, int v) {
        Mapper004::write(t,a,v);
        /* Mapper004::write() calls its own non-virtual update_* methods.
           Reapply the derived board mapping after all MMC3 bank/mode writes. */
        if ((a & 0xe001) == 0x8000 || (a & 0xe001) == 0x8001) {
            update_prg_banks();
        }
        if ((a & 0xe001) == 0xe000) irq_changed();
    }
    void write(nes_time_t t,nes_addr_t a,int v) {
        if (Id==114) {
            switch(a&0xe001){
            case 0x8001:base_write(t,0xa000,v);return;
            case 0xa000:base_write(t,0x8000,(v&0xc0)|security()[v&7]);e.r[1]=1;return;
            case 0xa001:run_until(t);irq_latch=(uint8_t)v;irq_changed();return;
            case 0xc000:if(e.r[1]){e.r[1]=0;base_write(t,0x8001,v);}return;
            case 0xc001:
                run_until(t);
                e.r[6] = 1; /* MMC3A reload requested on next A12 */
                irq_changed(); return;
            case 0xe000:case 0xe001:base_write(t,a,v);irq_changed();return;
            } return;
        }
        if(Id==121) {
            if(a<0xa000) {
                if((a&3)==3){
                    e.r[5]=(uint8_t)v;
                    switch(v&63){
                    case 0x20:case 0x29:case 0x2b:case 0x3c:case 0x3f:e.r[7]=1;e.r[0]=e.r[6];break;
                    case 0x26:e.r[7]=0;e.r[0]=e.r[6];break;
                    case 0x2c:e.r[7]=1;if(e.r[6])e.r[0]=e.r[6];break;
                    case 0x28:e.r[7]=0;e.r[1]=e.r[6];break;
                    case 0x2a:e.r[7]=0;e.r[2]=e.r[6];break;
                    case 0x2f:break;
                    default:e.r[5]=0;break;
                    }
                    base_write(t,0x8000,v);return;
                }
                if(a&1){
                    e.r[6]=((v&1)<<5)|((v&2)<<3)|((v&4)<<1)|((v&8)>>1)|((v&16)>>3)|((v&32)>>5);
                    if(!e.r[7]) {
                        switch(e.r[5]&63){
                        case 0x20:case 0x29:case 0x2b:case 0x3c:case 0x3f:e.r[0]=e.r[6];break;
                        case 0x28:e.r[1]=e.r[6];break;
                        case 0x2a:e.r[2]=e.r[6];break;
                        }
                    }
                    base_write(t,0x8001,v);return;
                }
                base_write(t,0x8000,v);return;
            }
            base_write(t,a,v);return;
        }
        if(Id==123 && a<0xa000) {
            base_write(t,(a&1)?0x8001:0x8000,(a&1)?v:((v&0xc0)|security()[v&7]));return;
        }
        if(Id==215) {
            static const uint8_t alut[5][8]={
                {0,1,2,3,4,5,6,7},
                {3,2,0,4,1,5,6,7},
                {0,1,2,3,4,5,6,7},
                {5,0,1,2,3,7,6,4},
                {3,1,0,5,2,4,6,7}
            };
            static const uint8_t vlut[5][8]={
                {0,1,2,3,4,5,6,7},
                {0,2,6,1,7,3,4,5},
                {0,5,4,1,7,2,6,3},
                {0,6,3,7,5,2,4,1},
                {0,2,5,3,6,1,7,4}
            };
            const int row=(e.r[2]<5)?e.r[2]:0;
            const int idx=((a>>12)&6)|(a&1);
            const int k=alut[row][idx];
            if(k==0) v=(v&0xc0)|vlut[row][v&7];
            a=(nes_addr_t)(0x8000|((k&6)<<12)|(k&1));
        }
        base_write(t,a,v);
    }
};

typedef AuroraMmc3Variant<114> Mapper114;
typedef AuroraMmc3Variant<115> Mapper115;
typedef AuroraMmc3Variant<121> Mapper121;
typedef AuroraMmc3Variant<123> Mapper123;
typedef AuroraMmc3Variant<189> Mapper189;
typedef AuroraMmc3Variant<197> Mapper197;
typedef AuroraMmc3Variant<215> Mapper215;
