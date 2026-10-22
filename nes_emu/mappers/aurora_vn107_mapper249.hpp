#pragma once
/* Aurora vN107 - Waixing T9552 (iNES 249 / NES2 MMC3 submapper 5).
 * Two dump bank orders are explicitly distinguished. No mapper 4
 * regression: the mapper 4 submapper-5 case is opt-in only.
 * Algorithm adapted from FCEUmm mapper249.c bank permutations.
 */
#include "mapper004.hpp"
#include <stdint.h>

class AuroraVn107Mapper249 : public Mapper004 {
    uint8_t scrambling_;
    bool nes2_canonical_;
    static unsigned reorder(unsigned value, const uint8_t* src, const uint8_t* dst, unsigned n) {
        unsigned bank = value;
        for (unsigned i=0;i<n;++i) bank &= ~(1u << dst[i]);
        for (unsigned i=0;i<n;++i) if (value & (1u << src[i])) bank |= (1u << dst[i]);
        return bank;
    }
    unsigned prg(unsigned page) const {
        static const uint8_t pattern[4][4]={{3,4,2,1},{4,3,1,2},{1,2,3,4},{2,1,4,3}};
        return reorder(page,pattern[scrambling_ & 3],pattern[nes2_canonical_?2:0],4);
    }
    unsigned chr(unsigned page) const {
        static const uint8_t pattern[8][6]={{5,2,6,7,4,3},{4,5,3,2,7,6},
            {2,3,4,5,6,7},{6,4,2,3,7,5},{5,3,7,6,2,4},
            {4,2,5,6,7,3},{3,6,4,5,2,7},{2,5,6,7,3,4}};
        return reorder(page,pattern[scrambling_ & 7],pattern[nes2_canonical_?2:0],6);
    }
public:
    explicit AuroraVn107Mapper249(bool canonical=false) : scrambling_(0),nes2_canonical_(canonical) {}
    void reset_state() { Mapper004::reset_state();scrambling_=0; }
    void save_state(mapper_state_t& out) {
        Mapper004::save_state(out);
        if(out.size==(int)sizeof(mmc3_state_t)) out.data[out.size++]=scrambling_;
    }
    void read_state(mapper_state_t const& in) {
        scrambling_ = in.size > (int)sizeof(mmc3_state_t)?in.data[sizeof(mmc3_state_t)]:0;
        Nes_Mapper::read_state(in);
    }
    void apply_mapping() {
        intercept_writes(0x5000,0x1000);
        Mapper004::apply_mapping();
        update_prg_banks(); // base PRG writer is deliberately non-virtual
    }
    bool write_intercepted(nes_time_t,nes_addr_t addr,int data) {
        if ((unsigned)(addr-0x5000)>=0x1000U) return false;
        scrambling_=(uint8_t)data;
        update_prg_banks();
        update_chr_banks();
        return true;
    }
    void write(nes_time_t time,nes_addr_t addr,int data) {
        Mapper004::write(time,addr,data);
        // Mapper004 calls its own non-virtual PRG updater. Reapply the
        // T9552 permutation after either MMC3 bank or mode write.
        const unsigned reg=(unsigned)addr & 0xe001U;
        if(reg==0x8000U || reg==0x8001U) update_prg_banks();
    }
    void update_prg_banks() {
        const int count=(int)(cart().prg_size() >> 13);
        if(count<2) return;
        const int last=count-1;
        int address=0x8000+((mode>>6&1)?0x4000:0);
        set_prg_bank(address,bank_8k,(int)prg(banks[6]));
        set_prg_bank(address^0x4000,bank_8k,(int)prg(last-1));
        set_prg_bank(0xa000,bank_8k,(int)prg(banks[7]));
        set_prg_bank(0xe000,bank_8k,(int)prg(last));
    }
    void update_chr_banks() {
        const int flip=(mode>>7&1)?0x1000:0;
        for(int i=0;i<2;++i) {
            unsigned v=(unsigned)(banks[i]&~1);
            for(int b=0;b<2;++b)
                set_chr_bank((i*0x800+b*0x400)^flip,bank_1k,(int)chr(v+b));
        }
        for(int i=0;i<4;++i)
            set_chr_bank((0x1000+i*0x400)^flip,bank_1k,(int)chr(banks[i+2]));
    }
};