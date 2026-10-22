#ifndef AURORA_VN66_MAPPER540_QUICKNES_CANDIDATE_HPP
#define AURORA_VN66_MAPPER540_QUICKNES_CANDIDATE_HPP
/* EXPERIMENTAL QUICKNES FRONT-END: NES 2.0 mapper 540 / UNL-82112C.
 * Standalone vN66 candidate, intentionally NOT in Nes_Mapper.cpp factory.
 * Mapping, CPU-IRQ, FDS expansion audio and a sound channel are connected
 * through existing Nes_Mapper/Blip_Buffer APIs. ROM IRQ timing,
 * target compiler integration, ROM verification and gain calibration remain
 * UNVALIDATED. Do not ship or register on the basis of synthetic tests.
 * Model reference: FCEUmm mapper359.c and fdssound.c (GPL-2.0-or-later).
 */
#include "Nes_Mapper.h"
#include "Blip_Buffer.h"
#include "aurora_vn92_mapper540_fds_bridge_STAGING.hpp"
#include <string.h>

class AuroraVn66Mapper540Candidate : public Nes_Mapper {
    AuroraMapper540FDSBridgeStage logic_;
    enum { ppu_first_scanline = 20*341+268, ppu_last_scanline = ppu_first_scanline+240*341 };
    // One callback per active pre-render/visible scanline, matching the
    // GameHBIRQHook model in FCEUmm, not a fabricated CHR A12 transition.
    uint8_t snapshot_[AuroraMapper540FDSBridgeStage::snapshot_size+4];
    nes_time_t next_ppu_scanline_;
    bool scanline_clock_active() const {
        return (emu().ppu.w2001 & 0x18) &&
               ((emu().ppu.w2000 & 0x38) != 0x18);
    }
    void run_scanline_clock_until(nes_time_t end_time) {
        const bool active = scanline_clock_active();
        const nes_time_t ppu_time = end_time*ppu_overclock;
        while(next_ppu_scanline_ < ppu_time && next_ppu_scanline_ <= ppu_last_scanline) {
            if(active)logic_.on_verified_ppu_irq_clock();
            next_ppu_scanline_ += Nes_Ppu::scanline_len;
        }
    }

    Blip_Buffer* output_;
    Blip_Synth<blip_good_quality,1> synth_;
    nes_time_t cursor_;
    int last_amplitude_;

    void sync_prg() {
        AuroraMapper540Stage::Mapping m;
        logic_.mapper().map(m);
        for (unsigned i=0;i<5;++i)
            set_prg_bank((nes_addr_t)(0x6000U+i*0x2000U),bank_8k,(int)m.prg8[i]);
    }
    void sync_chr() {
        AuroraMapper540Stage::Mapping m;
        logic_.mapper().map(m);
        // FCEUmm mapper359.c::SyncCHR: for BOTH 359 and 540, an
        // image without CHR-ROM uses a single 8 KiB CHR-RAM bank (0).
        // The four CHR registers are ignored in that mode.
        if (!cart().chr_size()) {
            set_chr_ram_bank(0,bank_8k,0);
        } else {
            for (unsigned i=0;i<4;++i)
                set_chr_bank((nes_addr_t)(i*0x800U),bank_2k,(int)m.chr2[i]);
        }
    }
    void sync_mirror() {
        AuroraMapper540Stage::Mapping m;
        logic_.mapper().map(m);
        mirror_manual((int)m.mirror[0],(int)m.mirror[1],
                      (int)m.mirror[2],(int)m.mirror[3]);
    }
    void sync_mapping() {sync_prg();sync_chr();sync_mirror();}
    bool write_register(nes_time_t time, nes_addr_t addr,int v) {
        run_until(time);
        const bool old_irq=logic_.irq_pending();
        const bool handled=logic_.write(addr,(unsigned)v);
        if(!handled) return false;
        // Match FCEUmm's WritePRG/WriteReg/WriteCHR: do not remap
        // PRG (and flush extra PPU time) on ordinary CHR bank writes.
        // In CHR-RAM mode the CHR registers still accept writes, but
        // they are ignored by physical CHR mapping (see sync_chr).
        switch (addr & 0xf000U) {
        case 0x8000U: sync_prg(); break;
        case 0x9000U: sync_prg();sync_chr();sync_mirror(); break;
        case 0xa000U: case 0xb000U: sync_chr(); break;
        default: break;
        }
        if(old_irq!=logic_.irq_pending() || (addr>=0xc000U && addr<0xd000U))
            irq_changed();
        return true;
    }
public:
    AuroraVn66Mapper540Candidate() : next_ppu_scanline_(ppu_first_scanline),output_(NULL),cursor_(0),last_amplitude_(0) {
        register_state(snapshot_,sizeof snapshot_);
        // The source model emits pre-mixer amplitudes, not direct PCM.
        // This tentative gain needs comparison with authentic FDS hardware.
        /* Blip_Synth::volume() expects Q30 FIXED POINT, not a double.\n         * The old 1.0/256.0 implicitly converted to integer zero, muting FDS.\n         * Preserve the intended gain without changing clock or sample rates. */\n        synth_.volume(BLIP_Q30_ONE / 256);
    }
    // PPUCTRL and PPUMASK changes already flush mapper time via QuickNES
    // when this is true; no global renderer alteration is required.
    virtual bool ppu_ctrl_mask_irq_sensitive() const {return true;}
    virtual int channel_count() const {return 1;}
    virtual void set_channel_buf(int i, Blip_Buffer* b) {
        if(i!=0 || b==output_)return;
        // A newly attached Blip_Buffer begins with zero DC reference. Do not
        // carry the old buffer's last emitted amplitude into another stream.
        // Continue clocking the ASIC even while the sound channel is detached.
        output_=b;
        last_amplitude_=0;
    }
    virtual void set_treble(blip_eq_t const& eq) {synth_.treble_eq(eq);}
    virtual void reset_state() {
        logic_=AuroraMapper540FDSBridgeStage();
        cursor_=0;
        next_ppu_scanline_=ppu_first_scanline;
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
            if(output_) {
                if(delta)synth_.offset(t,delta,output_);
                last_amplitude_=amp;
            } else {
                // Detached output has no DC history; reattach from zero.
                last_amplitude_=0;
            }
        }
        run_scanline_clock_until(end_time);
        cursor_=end_time;
        if(old_irq!=logic_.irq_pending())irq_changed();
    }
    virtual nes_time_t next_irq(nes_time_t present) {
        run_until(present);
        if(logic_.irq_pending())return present;
        // CPU counter supports accurate next-interrupt prediction.
        if(logic_.mapper().uses_ppu_irq()) {
            // Reference mapper uses one HBlank hook, guarded by rendering
            // and PPUCTRL state. Predict against a COPY: next_irq must never
            // consume future counter clocks or mutate mapped state.
            if(!scanline_clock_active())return no_irq;
            AuroraMapper540Stage prediction=logic_.mapper();
            for(nes_time_t scan=next_ppu_scanline_;scan<=ppu_last_scanline;
                scan+=Nes_Ppu::scanline_len) {
                prediction.ppu_irq_clock();
                if(prediction.pending_irq())return scan/ppu_overclock+1;
            }
            return no_irq;
        }
        if(logic_.mapper().cpu_irq_active()) {
            const unsigned count=logic_.mapper().cpu_count();
            return cursor_+(nes_time_t)((count&0x8000U)?1U:count);
        }
        return no_irq;
    }
    virtual void end_frame(nes_time_t length) {
        run_until(length);
        cursor_-=length;
        next_ppu_scanline_=ppu_first_scanline;
    }
    virtual void save_state(mapper_state_t& out) {
        // QuickNES can save at a CPU timestamp after the last register
        // access. Flush ASIC, FDS waveform and pending IRQ to that point.
        // Use Nes_Mapper::mapper_clock(): Nes_Core::clock() itself is private
        // and its friendship with the base class is NOT inherited.
        const nes_time_t now=mapper_clock();
        if (now>cursor_)run_until(now);
        logic_.save(snapshot_);
        const uint32_t pos=(uint32_t)next_ppu_scanline_;
        for(unsigned i=0;i<4;++i)snapshot_[AuroraMapper540FDSBridgeStage::snapshot_size+i]=(uint8_t)(pos>>(8*i));
        Nes_Mapper::save_state(out);
    }
    virtual void read_state(mapper_state_t const& in) {
        memset(snapshot_,0,sizeof snapshot_);
        if(in.size!=(long)sizeof snapshot_) {reset_state();return;}
        in.read(snapshot_,sizeof snapshot_);
        if(!logic_.load(snapshot_))reset_state();
        uint32_t pos=0;
        for(unsigned i=0;i<4;++i)pos|=(uint32_t)snapshot_[AuroraMapper540FDSBridgeStage::snapshot_size+i]<<(8*i);
        // Only boundary-aligned scanline positions are valid. In particular,
        // reject a corrupted mid-line timestamp instead of skipping an IRQ.
        if(pos<(uint32_t)ppu_first_scanline ||
           pos>(uint32_t)(ppu_last_scanline+Nes_Ppu::scanline_len) ||
           ((pos-(uint32_t)ppu_first_scanline) % Nes_Ppu::scanline_len)!=0U)
            reset_state();
        else next_ppu_scanline_=(nes_time_t)pos;
        cursor_=0;
        // A savestate replaces the ASIC's internal waveform but NOT the
        // Blip_Buffer's accumulated DC offset. Keep the amplitude last sent
        // to that buffer; the next tick emits the difference to the restored
        // ASIC amplitude. Reset only when output is disconnected.
        if(!output_)last_amplitude_=0;
    }
    // Remaining limitations: schedule is QuickNES/FCEUmm scanline-level,
    // not an independently verified pixel-dot model; requires ROM testing.
};
#endif