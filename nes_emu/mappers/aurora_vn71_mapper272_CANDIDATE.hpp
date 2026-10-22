#ifndef AURORA_VN71_MAPPER272_QUICKNES_CANDIDATE_HPP
#define AURORA_VN71_MAPPER272_QUICKNES_CANDIDATE_HPP
/* Experimental QuickNES bridge for mapper 272. Physical pattern fetch callbacks
   in QuickNES are not yet complete: not production-ready. */
#include "Nes_Mapper.h"
#include "Nes_Core.h"
#include "aurora_vn90_mapper272_model.hpp"
#include <string.h>

class Mapper272 : public Nes_Mapper {
    AuroraMapper272Stage m_;
    uint8_t packed_[AuroraMapper272Stage::snapshot_size];
    void sync_prg(){
        AuroraMapper272Stage::Mapping z; m_.map(z,(unsigned)(cart().prg_size()/0x2000L));
        for(unsigned i=0;i<4U;++i) set_prg_bank((nes_addr_t)(0x8000U+0x2000U*i),bank_8k,(int)z.prg8[i]);
    }
    void sync_chr(){
        AuroraMapper272Stage::Mapping z; m_.map(z,(unsigned)(cart().prg_size()/0x2000L));
        for(unsigned i=0;i<8U;++i) set_chr_bank((nes_addr_t)(0x400U*i),bank_1k,(int)z.chr1[i]);
    }
    void sync_nt(){
        AuroraMapper272Stage::Mapping z; m_.map(z,(unsigned)(cart().prg_size()/0x2000L));
        mirror_manual(z.nt[0],z.nt[1],z.nt[2],z.nt[3]);
    }
    void sync(){sync_prg();sync_chr();sync_nt();}
public:
    Mapper272(){memset(packed_,0,sizeof packed_);register_state(packed_,sizeof packed_);}
    virtual void reset_state(){m_.reset();}
    // vN95: QuickNES' vram_address_changed is a CPU-side $2006/$2007
    // notification, NOT a physical rendering fetch. Never clock PAL IRQ there.
    virtual bool needs_vram_address_hook() const{return false;}
    // Deliberately unconnected until a verified physical PPU fetch hook exists.
    void aurora_physical_ppu_fetch_event(nes_addr_t addr){
        const bool old=m_.pending_irq();
        m_.ppu_fetch((unsigned)addr);
        if(old!=m_.pending_irq())irq_changed();
    }
    virtual void apply_mapping(){sync();}
    virtual void write(nes_time_t t,nes_addr_t addr,int data){
        (void)t;
        const bool old=m_.pending_irq();
        if(!m_.write((unsigned)addr,(unsigned)data))return;
        switch(addr&0xf000U){
        case 0x8000:case 0xa000:sync_prg();break;
        case 0x9000:sync_nt();break;
        case 0xb000:case 0xc000:case 0xd000:case 0xe000:sync_chr();break;
        }
        if((addr&0xc00cU)==0x8004U)sync_nt();
        if(old!=m_.pending_irq())irq_changed();
    }
    virtual nes_time_t next_irq(nes_time_t t){return m_.pending_irq()?t:no_irq;}
    virtual void save_state(mapper_state_t& out){m_.save(packed_);Nes_Mapper::save_state(out);}
    virtual void read_state(mapper_state_t const& in){
        memset(packed_,0,sizeof packed_);in.read(packed_,sizeof packed_);
        m_.load(packed_);sync();
    }
};
#endif