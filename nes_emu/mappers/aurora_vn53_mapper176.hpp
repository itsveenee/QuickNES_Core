#pragma once
/* SNESticleAurora QuickNES vN53: mapper 176 FK23C/FK-xxx subset.
 * Basis: FCEUmm-next mapper176.c, NES2 submappers 0 and 1 only.
 * NO DIP rotation, no NOAC/FS005/JX9003B/Gamestar/HST hardware.
 * True MMC3 A12 approximation remains inherited from Mapper004.
 * No shared CPU/PPU modifications. CHR-RAM mixed ROM uses existing
 * QuickNES aux_chr_ram_size() facility (8 KiB); other variants guarded.
 */
#include "mapper004.hpp"
#include <stdint.h>
#include <string.h>

class AuroraVn53Mapper176 : public Mapper004 {
    struct Ext {
        uint8_t fk[4];
        uint8_t regs[4]; /* PRG 8,9 and CHR 10,11 */
        uint8_t latch;
        uint8_t wramctl;
        uint8_t ext_mirror;
    } ext;
    unsigned variant() const {
        unsigned sm=(unsigned)cart().submapper_code();
        if(sm!=0U) return sm;
        // FCEUmm legacy iNES size heuristics: these FK-xxx cartridges
        // predate NES2 submapper tagging. No physical DIP inference.
        const long p=cart().prg_size(),c=cart().chr_size();
        if((p==0x100000L && c==0x100000L) ||
           (p==0x40000L  && c==0x20000L) ||
           (p==0x20000L  && c==0x10000L)) return 1U;
        return 0U;
    }
    unsigned prg_mode() const { return ext.fk[0] & 7U; }
    bool extended() const { return (ext.fk[3] & 2U) != 0; }
    unsigned prg_reg(unsigned i) const {
        return i<8U ? banks[i] : ext.regs[i-8U];
    }
    unsigned chr_reg(unsigned i) const {
        return i<8U ? banks[i] : ext.regs[i-8U];
    }
    void map_prg8(nes_addr_t a,unsigned b) {set_prg_bank(a,bank_8k,(int)b);}
    virtual void update_prg_banks() {
        static const unsigned mask_lut[8]={0x3f,0x1f,0x0f,0,0,0,0x7f,0xff};
        unsigned pm=prg_mode(),mask=mask_lut[pm];
        unsigned base=ext.fk[1]&0x7fU;
        if (variant()==1 && (pm==0 || extended())) mask=0xffU;
        if (pm==3U) {
            map_prg8(0x8000,(base<<1)|0U);
            map_prg8(0xa000,(base<<1)|1U);
            map_prg8(0xc000,(base<<1)|0U);
            map_prg8(0xe000,(base<<1)|1U);
        } else if (pm==4U) {
            for(unsigned i=0;i<4;++i) map_prg8(0x8000+i*0x2000U,((base>>1)<<2)|i);
        } else if (pm==5U) {
            unsigned b=(base&~7U)|(ext.latch&7U);
            map_prg8(0x8000,b*2);
            map_prg8(0xa000,b*2+1U);
            b=(base&~7U)|(ext.latch&7U)|7U;
            map_prg8(0xc000,b*2);
            map_prg8(0xe000,b*2+1U);
        } else {
            base<<=1;
            const unsigned a0=(mode&0x40)?0xc000U:0x8000U;
            map_prg8(a0,(base&~mask)|(prg_reg(6)&mask));
            map_prg8(0xa000,(base&~mask)|(prg_reg(7)&mask));
            map_prg8(a0^0x4000U,(base&~mask)|((extended()?prg_reg(8):0xfeU)&mask));
            map_prg8(0xe000,(base&~mask)|((extended()?prg_reg(9):0xffU)&mask));
        }
    }
    virtual void update_chr_banks() {
        unsigned mask=(ext.fk[0]&0x10U)?0x7fU:0xffU;
        unsigned base=(unsigned)ext.fk[2]<<3;
        unsigned val[8];
        if (ext.fk[0]&0x40U) {
            mask = (variant()==1 && !(ext.fk[0]&0x20U)) ?
                ((ext.fk[0]&0x10U)?1U:3U):0U;
            base=(base&~(mask<<3))|((unsigned(ext.latch)&mask)<<3);
            for(unsigned i=0;i<8;++i)val[i]=base|i;
        } else if (extended()) {
            val[0]=(base&~mask)|(prg_reg(0)&mask);
            val[1]=(base&~mask)|(chr_reg(10)&mask);
            val[2]=(base&~mask)|(prg_reg(1)&mask);
            val[3]=(base&~mask)|(chr_reg(11)&mask);
            for(unsigned i=4;i<8;++i)val[i]=(base&~mask)|(prg_reg(i-2)&mask);
        } else {
            val[0]=(base&~mask)|((unsigned(banks[0])&0xfeU)&mask);
            val[1]=(base&~mask)|((unsigned(banks[0])|1U)&mask);
            val[2]=(base&~mask)|((unsigned(banks[1])&0xfeU)&mask);
            val[3]=(base&~mask)|((unsigned(banks[1])|1U)&mask);
            for(unsigned i=4;i<8;++i)val[i]=(base&~mask)|(unsigned(banks[i-2])&mask);
        }
        const unsigned swap=(mode&0x80U)?4U:0U;
        bool mixed=(cart().chr_size()!=0 && (ext.fk[0]&0x20U));
        for(unsigned i=0;i<8;++i){
            const nes_addr_t addr=(i^swap)*0x400U;
            if(mixed) set_chr_ram_bank(addr,bank_1k,(int)val[i]);
            else set_chr_bank(addr,bank_1k,(int)val[i]);
        }
    }
    void do_mirror() {
        if (cart().mirroring()&0x08) return;
        if(ext.ext_mirror & 1U)mirror_horiz(); else mirror_vert();
    }
public:
    AuroraVn53Mapper176(){memset(&ext,0,sizeof ext);}
    virtual long aux_chr_ram_size() const {return cart().chr_size()?0x2000L:0L;}
    virtual void reset_state() {
        Mapper004::reset_state();
        memset(&ext,0,sizeof ext);
        ext.regs[0]=0xfe;ext.regs[1]=0xff;
        ext.regs[2]=0xff;ext.regs[3]=0xff;
        ext.wramctl=0x80;
        if(variant()==1U) ext.fk[1]=0xff;
        mode=0;
        sram_mode=0x80;
    }
    virtual void apply_mapping() {
        // Base apply_mapping dispatches synthetic $A000/$A001 writes;
        // this board latches every high-ROM write, so do not use it.
        update_prg_banks();
        update_chr_banks();
        start_frame();
        intercept_writes(0x5000,0x1000);
        if(ext.wramctl&0x80)enable_sram(true,(ext.wramctl&0x40)!=0);
        do_mirror();
    }
    virtual bool write_intercepted(nes_time_t,nes_addr_t a,int v) {
        if(a>=0x5000U&&a<=0x5fffU){
            // $501x selects FK23C registers; physical DIP address rotation
            // is intentionally not implemented.
            if(a&0x10U){ext.fk[a&3U]=uint8_t(v);update_prg_banks();update_chr_banks();}
            return true;
        }
        return false;
    }
    virtual void write(nes_time_t t,nes_addr_t a,int d) {
        const uint8_t v=(uint8_t)d;
        if(a<0x8000U)return;
        if(ext.latch!=v){ext.latch=v;
            if(ext.fk[0]&0x40U)update_chr_banks();
            if(prg_mode()==5U)update_prg_banks();
        }
        switch(a&0xf000U){
        case 0x8000:case 0x9000:
            if(a&2U)break; // $8002/3 ignored by this board
            if(a&1U){
                unsigned index=mode&(extended()?15U:7U);
                if(index<=7U)banks[index]=v;
                else if(index<=11U)ext.regs[index-8U]=v;
                if(index==6U || index==7U || index==8U || index==9U)update_prg_banks();
                else update_chr_banks();
            }else{
                const uint8_t before=mode;
                mode=v;
                if((before^mode)&0x40U)update_prg_banks();
                if((before^mode)&0x80U)update_chr_banks();
            }
            break;
        case 0xa000:case 0xb000:
            if(a&1U){
                ext.wramctl=v;
                sram_mode=v;
                enable_sram((v&0x80U)!=0,(v&0x40U)!=0);
                update_chr_banks();
            }else {ext.ext_mirror=v;do_mirror();}
            break;
        default:
            Mapper004::write(t,a,d); // inherited MMC3 IRQ: C000..FFFF
            break;
        }
    }
    virtual void save_state(mapper_state_t&out){
        uint8_t b[sizeof(mmc3_state_t)+sizeof(Ext)];
        memcpy(b,static_cast<const mmc3_state_t*>(this),sizeof(mmc3_state_t));
        memcpy(b+sizeof(mmc3_state_t),&ext,sizeof ext);
        out.write(b,sizeof b);
    }
    virtual void read_state(mapper_state_t const&in){
        uint8_t b[sizeof(mmc3_state_t)+sizeof(Ext)];memset(b,0,sizeof b);
        in.read(b,sizeof b);
        memcpy(static_cast<mmc3_state_t*>(this),b,sizeof(mmc3_state_t));
        memcpy(&ext,b+sizeof(mmc3_state_t),sizeof ext);
        /* Nes_Mapper::load_state reapplies mapping after restoration. */
    }
};
