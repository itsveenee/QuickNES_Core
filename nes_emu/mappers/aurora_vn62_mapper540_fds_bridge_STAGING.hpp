#ifndef AURORA_VN62_MAPPER540_FDS_BRIDGE_STAGING_HPP
#define AURORA_VN62_MAPPER540_FDS_BRIDGE_STAGING_HPP
/* Combined CPU-IRQ and FDS sound model; experimental. */
#include "aurora_vn62_mapper540_core_STAGING.hpp"
#include "aurora_vn61_fds_sound_STAGING.hpp"
#include <stdint.h>
#include <string.h>
class AuroraMapper540FDSBridgeStage {
    AuroraMapper540Stage map_;
    AuroraFDSSoundStage fds_;
public:
    enum { snapshot_size=AuroraMapper540Stage::snapshot_size+1+AuroraFDSSoundStage::state_size };
    AuroraMapper540FDSBridgeStage() {}
    AuroraMapper540Stage& mapper() {return map_;}
    const AuroraMapper540Stage& mapper() const {return map_;}
    bool irq_pending() const {return map_.pending_irq();}
    bool write(unsigned addr,unsigned val) {
        if(addr>=0x4040U && addr<=0x408aU)return fds_.write(addr,val);
        return map_.write(addr,val);
    }
    bool read(unsigned addr,unsigned& val) const {return fds_.read(addr,val);}
    void clock_cpu(unsigned cycles) {
        map_.cpu_cycles(cycles);
        for(unsigned i=0;i<cycles;++i) {fds_.tick_half_cpu();fds_.tick_half_cpu();}
    }
    int sample_pre_mixer() const {return (int)fds_.current_sample();}
    void save(uint8_t out[snapshot_size]) const {
        map_.save(out);
        out[AuroraMapper540Stage::snapshot_size]=(uint8_t)map_.pending_irq();
        fds_.save(out+AuroraMapper540Stage::snapshot_size+1);
    }
    bool load(const uint8_t in[snapshot_size]) {
        map_.load(in);
        map_.set_irq_line(in[AuroraMapper540Stage::snapshot_size]!=0);
        return fds_.load(in+AuroraMapper540Stage::snapshot_size+1);
    }
};
#endif
