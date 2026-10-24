#ifndef AURORA_VN61_MAPPER540_FDS_BRIDGE_STAGING_HPP
#define AURORA_VN61_MAPPER540_FDS_BRIDGE_STAGING_HPP
/* STAGING / mapper 540 controller + FDS register and envelope clock bridge.
 * No QuickNES mixer, Blip_Buffer or IRQ delivery is connected here.
 * Do not install this as a complete mapper. No sample rate is selected;
 * caller MUST supply elapsed emulated CPU cycles in correct order.
 */
#include "aurora_vn92_mapper540_core_STAGING.hpp"
#include "aurora_vn61_fds_sound_STAGING.hpp"
class AuroraMapper540FDSBridgeStage {
public:
    enum { snapshot_size = 24 + AuroraFDSSoundStage::state_size + 1 };
private:
    AuroraMapper540Stage mapper_;
    AuroraFDSSoundStage audio_;
    uint32_t total_cycles_;
public:
    AuroraMapper540FDSBridgeStage():total_cycles_(0) {}
    bool write(unsigned addr,unsigned v) {
        if (addr>=0x4040U && addr<=0x408aU) return audio_.write(addr,v);
        return mapper_.write(addr,v);
    }
    bool read(unsigned addr,unsigned& result) const {
        if(addr>=0x4040U && addr<=0x4092U)return audio_.read(addr,result);
        return false;
    }
    // The caller supplies emulated CPU cycles, not audio-output samples.
    void clock_cpu(unsigned cycles) {
        mapper_.cpu_cycles(cycles);
        for(unsigned i=0;i<cycles;++i) {
            audio_.tick_half_cpu();
            audio_.tick_half_cpu();
        }
        total_cycles_ += cycles;
    }
    void on_verified_ppu_irq_clock(){mapper_.ppu_irq_clock();}
    bool irq_pending()const{return mapper_.pending_irq();}
    int32_t sample_pre_mixer()const{return audio_.current_sample();}
    unsigned long cycles_run()const{return total_cycles_;}
    AuroraMapper540Stage& mapper() {return mapper_;}
    AuroraFDSSoundStage& audio() {return audio_;}
    void save(uint8_t out[snapshot_size])const {
        mapper_.save(out);
        audio_.save(out+24);
        out[24+AuroraFDSSoundStage::state_size]=mapper_.pending_irq()?1:0;
    }
    bool load(const uint8_t in[snapshot_size]) {
        // Validate source sound state before mutating the mapper or sound.
        AuroraFDSSoundStage parsed=audio_;
        if(!parsed.load(in+24))return false;
        mapper_.load(in);
        mapper_.set_irq_line(in[24+AuroraFDSSoundStage::state_size]!=0);
        audio_=parsed;
        total_cycles_=0; // scheduler epoch is not stored: owner restores it
        return true;
    }
};
#endif