#ifndef AURORA_VN71_MAPPER558_QUICKNES_CANDIDATE_HPP
#define AURORA_VN71_MAPPER558_QUICKNES_CANDIDATE_HPP
/* Experimental NES2 mapper 558 QuickNES bridge. Requires accurate PA09/PA13
   pattern-fetch stream and NES2 PRG RAM+EEPROM persistence metadata.
   No false claims of complete EEPROM persistence or PPU timing. */
#include "Nes_Mapper.h"
#include "Nes_Core.h"
#include "aurora_vn112_mapper558_model.hpp"
#include <string.h>

class Mapper558 : public Nes_Mapper {
    AuroraMapper558Stage m_;
    bool visual_1bpp_mode_;
    uint8_t packed_[AuroraMapper558Stage::snapshot_size];
    void sync_prg(){
        AuroraMapper558Stage::Mapping z;m_.map(z);
        set_prg_bank(0x8000,bank_32k,(int)z.prg32);
        enable_sram(true);
    }
    void sync_chr(){
        /* vN191: normal YC-03-09 uses a whole 8 KiB CHR bank. The old
         * implementation erroneously mirrored its first 4 KiB into both
         * halves, breaking ordinary two-plane backgrounds.
         * 1bpp remains an experimental visualization: map distinct 4 KiB
         * halves and let the verified PPU renderer derive plane from NT A0.
         * Accurate PA13/PA09 fetch notifications are still required for
         * CPU $2007 latch behavior and the complete cartridge semantics. */
        const bool mode=m_.video_1bpp_enabled();
        /* vN195: finish rendering through the current CPU instant before
         * switching the physical 1bpp wiring. Do nothing for repeated
         * writes that leave the mode unchanged. */
        if(mode!=visual_1bpp_mode_) {
            emu().ppu.render_until(emu().clock());
            emu().ppu.set_pec586_1bpp_mode(mode);
            visual_1bpp_mode_=mode;
        }
        if(!mode){
            if(cart().chr_size())set_chr_bank(0x0000,bank_8k,0);
            else set_chr_ram_bank(0x0000,bank_8k,0);
            return;
        }
        if(cart().chr_size()){
            set_chr_bank(0x0000,bank_4k,0);
            set_chr_bank(0x1000,bank_4k,1);
        }else{
            set_chr_ram_bank(0x0000,bank_4k,0);
            set_chr_ram_bank(0x1000,bank_4k,1);
        }
    }
public:
    Mapper558():m_(false),visual_1bpp_mode_(false){memset(packed_,0,sizeof packed_);register_state(packed_,sizeof packed_);}
    virtual void reset_state(){m_.reset();visual_1bpp_mode_=false;emu().ppu.set_pec586_1bpp_mode(false);}
    virtual long aurora_extra_sram_save_size() const {
        // Bank 0 contains 512-byte EEPROM backing; bank 1 contains 8 KiB
        // volatile WRAM. Store both in savestates, but only EEPROM in .srm.
        return cart().battery_ram_size()==512 ? 0x4000L : 0L;
    }
    // vN95: Do not confuse CPU $2006/$2007 notifications with physical
    // PPU pattern/nametable fetches. This callback remains unconnected.
    virtual bool needs_vram_address_hook()const{return false;}
    void aurora_physical_ppu_fetch_event(nes_addr_t a){
        AuroraMapper558Stage::Mapping before,after;m_.map(before);
        m_.ppu_fetch((unsigned)a);m_.map(after);
        if(before.chr4[0]!=after.chr4[0]) {
            // Avoid render_until() recursion from a PPU address callback.
            // DO NOT replace with normal CHR-ROM bank calls on CHR-RAM carts.
            // Hardware 1bpp address remapping remains an unsolved PPU policy
            // prerequisite; vN95 only prevents wrong CPU notifications.
            if(cart().chr_size()) {
                emu().ppu.set_chr_bank(0,0x1000,(long)after.chr4[0]*0x1000L);
                emu().ppu.set_chr_bank(0x1000,0x1000,(long)after.chr4[1]*0x1000L);
            }
        }
    }
    virtual void apply_mapping(){
        const bool has93c66=cart().battery_ram_size()==512;
        m_.configure_eeprom(has93c66);
        // Non-battery WRAM must not overwrite the 512-byte EEPROM save area.
        if(has93c66) set_sram_bank(1);
        else set_sram_bank(0);
        sync_prg();
        if(has93c66) m_.bind_eeprom_storage(sram_data_ptr(),512);
        else m_.unbind_eeprom_storage();
        sync_chr();
        intercept_reads(0x5000,0x800);
        intercept_writes(0x5000,0x800);
    }
    virtual void write(nes_time_t,nes_addr_t,int){}
    virtual bool write_intercepted(nes_time_t,nes_addr_t a,int v){
        if(!m_.write((unsigned)a,(unsigned)v,(unsigned)cart().prg_size()))return false;
        if((a&0xff00U)!=0x5200U){sync_prg();sync_chr();}
        return true;
    }
    virtual int read(nes_time_t,nes_addr_t a){unsigned v;
        return m_.read((unsigned)a,v)?(int)v:-1;
    }
    virtual void save_state(mapper_state_t& out){m_.save(packed_);Nes_Mapper::save_state(out);}
    virtual void read_state(mapper_state_t const& in){
        memset(packed_,0,sizeof packed_);in.read(packed_,sizeof packed_);
        m_.load(packed_);sync_prg();sync_chr();
    }
};
#endif