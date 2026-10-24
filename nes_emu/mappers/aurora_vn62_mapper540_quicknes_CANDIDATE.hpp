#ifndef AURORA_VN62_MAPPER540_QUICKNES_CANDIDATE_HPP
#define AURORA_VN62_MAPPER540_QUICKNES_CANDIDATE_HPP
/* EXPERIMENTAL QUICKNES FRONT-END: NES 2.0 mapper 540 / UNL-82112C.
 * Standalone candidate, intentionally NOT in Nes_Mapper.cpp factory.
 * Mapping, CPU-IRQ, FDS expansion audio and a sound channel are connected
 * through existing Nes_Mapper/Blip_Buffer APIs. PPU scanline IRQ source,
 * target compiler integration, ROM verification and gain calibration remain
 * UNVALIDATED. Do not ship or register on the basis of synthetic tests.
 * Model reference: FCEUmm mapper359.c and fdssound.c (GPL-2.0-or-later).
 */
#include "Nes_Mapper.h"
#include "Blip_Buffer.h"
#include "aurora_vn62_mapper540_fds_bridge_STAGING.hpp"
#include <string.h>

class AuroraVn62Mapper540Candidate : public Nes_Mapper {
    AuroraMapper540FDSBridgeStage logic_;
    uint8_t snapshot_[AuroraMapper540FDSBridgeStage::snapshot_size];
    Blip_Buffer* output_;
    Blip_Synth<blip_good_quality,1> synth_;
    nes_time_t cursor_;
    int last_amplitude_;

    void sync_mapping() {
        AuroraMapper540Stage::Mapping m;
        logic_.mapper().map(m);
        for (unsigned i=0;i<5;++i)
            set_prg_bank((nes_addr_t)(0x6000U+i*0x2000U),bank_8k,(int)m.prg8[i]);
        for (unsigned i=0;i<4;++i) {
            if(cart().chr_size()) set_chr_bank((nes_addr_t)(i*0x800U),bank_2k,(int)m.chr2[i]);
            else set_chr_ram_bank((nes_addr_t)(i*0x800U),bank_2k,(int)m.chr2[i]);
        }
        mirror_manual((int)m.mirror[0],(int)m.mirror[1],
                      (int)m.mirror[2],(int)m.mirror[3]);
    }
    bool write_register(nes_time_t time, nes_addr_t addr,int v) {
        run_until(time);
        const bool old_irq=logic_.irq_pending();
        const bool handled=logic_.write(addr,(unsigned)v);
        if(!handled) return false;
        if((addr>=0x8000U && addr<0xc000U)) sync_mapping();
        if(old_irq!=logic_.irq_pending() || (addr>=0xc000U && addr<0xd000U))
            irq_changed();
        return true;
    }
public:
    AuroraVn62Mapper540Candidate() : output_(NULL),cursor_(0),last_amplitude_(0) {
        register_state(snapshot_,sizeof snapshot_);
        // The source model emits pre-mixer amplitudes, not direct PCM.
        // This tentative gain needs comparison with authentic FDS hardware.
        synth_.volume(1.0/256.0);
    }
    virtual int channel_count() const {return 1;}
    virtual void set_channel_buf(int i, Blip_Buffer* b) {
        if(i==0) output_=b;
    }
    virtual void set_treble(blip_eq_t const& eq) {synth_.treble_eq(eq);}
    virtual void reset_state() {
        logic_=AuroraMapper540FDSBridgeStage();
        cursor_=0;
        last_amplitude_=0;
        memset(snapshot_,0,sizeof snapshot_);
    }
    virtual void apply_mapping() {
        sync_mapping();
        intercept_reads(0x4000,0x1000);
        intercept_writes(0x4000,0x1000);
    }
    virtual bool write_intercepted(nes_time_t t,nes_addr_t a,int v) {
        if(a>=0x8000U) return false;
        return write_register(t,a,v);
    }
    virtual void write(nes_time_t t,nes_addr_t a,int v) {
        write_register(t,a,v);
    }
    virtual int read(nes_time_t t,nes_addr_t a) {
        unsigned v=0;
        if(a<0x4040U || a>0x4092U) return -1;
        run_until(t);
        return logic_.read(a,v) ? (int)v : -1;
    }
    virtual void run_until(nes_time_t end_time) {
        if(end_time<=cursor_)return;
        const bool old_irq=logic_.irq_pending();
        // Flush at emulated CPU-clock boundaries. Never use host sample rates.
        for(nes_time_t t=cursor_; t<end_time; ++t) {
            logic_.clock_cpu(1);
            const int amp=(int)logic_.sample_pre_mixer();
            const int delta=amp-last_amplitude_;
            if(delta && output_)synth_.offset(t,delta,output_);
            last_amplitude_=amp;
        }
        cursor_=end_time;
        if(old_irq!=logic_.irq_pending())irq_changed();
    }
    virtual nes_time_t next_irq(nes_time_t present) {
        run_until(present);
        if(logic_.irq_pending())return present;
        // CPU counter supports accurate next-interrupt prediction.
        // PPU-driven scanline mode is not predicted or artificially clocked.
        if(logic_.mapper().cpu_irq_active()) {
            const unsigned count=logic_.mapper().cpu_count();
            return cursor_+(nes_time_t)((count&0x8000U)?1U:count);
        }
        return no_irq;
    }
    virtual void end_frame(nes_time_t length) {
        run_until(length);
        cursor_-=length;
    }
    virtual void save_state(mapper_state_t& out) {
        logic_.save(snapshot_);
        Nes_Mapper::save_state(out);
    }
    virtual void read_state(mapper_state_t const& in) {
        memset(snapshot_,0,sizeof snapshot_);
        if(in.size!=(long)sizeof snapshot_) {reset_state();return;}
        in.read(snapshot_,sizeof snapshot_);
        if(!logic_.load(snapshot_))reset_state();
        cursor_=0;
        last_amplitude_=logic_.sample_pre_mixer();
    }
    // No invented per-scanline events; PPU IRQ mode deliberately incomplete.
};
#endif
