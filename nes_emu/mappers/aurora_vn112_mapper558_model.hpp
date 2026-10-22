#pragma once
/* Aurora vN57 / mapper 558 STAGING ONLY, NOT REGISTERED.
 * Hardware bus PA09/PA13 notification must be sourced from actual PPU fetch.
 * Serial 93C66 512x8 model uses physically gated write enable, CS/CLK edges;
 * deliberately not identical to historical FCEUmm capacity=uint8_t bug.
 * Reference: libretro-fceumm_next/src/mappers/mapper558.c and
 *            src/mappers/hw/eeprom_93Cx6.c (GPL-2.0-or-later).
 * Preserve upstream notices and applicable licenses on integration.
 */
#include <stdint.h>
#include <string.h>

class Aurora93C66Stage {
public:
    enum { capacity=512, snapshot_size=12 };
    enum State { WAIT_START=0, READ_OPCODE=1, READ_ADDRESS=2,
                 READ_WRITE_DATA=3, SHIFT_READ=4, FINISHED=5 };
private:
    uint8_t mem[capacity];
    uint8_t *external_storage;
    uint8_t *storage() { return external_storage ? external_storage : mem; }
    const uint8_t *storage() const { return external_storage ? external_storage : mem; }
    uint16_t address;
    uint8_t command, state, bit_count, data, last_clock, cs_active;
    uint8_t write_enabled, out;
    void finish() { state=FINISHED;out=0; }
    void perform_command() {
        if (command == 0) { // 00 + nine address bits; high two bits encode op
            switch ((address >> 7)&3U) {
            case 0: write_enabled=0;finish();break; // EWDS
            case 3: write_enabled=1;finish();break; // EWEN
            case 2: if(write_enabled) memset(storage(),0xff,capacity);finish();break; // ERAL
            default: state=READ_WRITE_DATA;bit_count=0;data=0;break; // WRAL
            }
        } else if(command==3) { // ERASE
            if(write_enabled) storage()[address & 511U]=0xff;
            finish();
        } else if(command==2) { // READ
            data=storage()[address&511U];
            state=SHIFT_READ;bit_count=0;
        } else {state=READ_WRITE_DATA;bit_count=0;data=0;} // WRITE
    }
public:
    Aurora93C66Stage() : external_storage(0) {memset(mem,0xff,sizeof mem);reset_protocol();}
    void reset_protocol() {
        address=0;command=0;state=WAIT_START;bit_count=0;data=0;
        last_clock=0;cs_active=0;write_enabled=0;out=1;
    }
    void signal(bool cs,bool clk,bool input) {
        const uint8_t c=(uint8_t)(cs!=0), k=(uint8_t)(clk!=0);
        if (!c) { // CS low aborts an instruction; EWEN latch survives
            state=WAIT_START;bit_count=0;command=0;address=0;
            last_clock=k;cs_active=0;out=1;
            return;
        }
        // Only an actual CLK low-to-high transition may advance the serial protocol.
        // A rising CS while CLK is already high is not a clock edge.
        bool rising=k && !last_clock;
        last_clock=k;cs_active=1;
        if (!rising) return;
        switch(state) {
        case WAIT_START:
            if(input) {state=READ_OPCODE;bit_count=0;command=0;out=1;}
            break;
        case READ_OPCODE:
            command=(uint8_t)((command<<1)|(input?1U:0U));
            if(++bit_count==2U){state=READ_ADDRESS;bit_count=0;address=0;}
            break;
        case READ_ADDRESS:
            address=(uint16_t)((address<<1)|(input?1U:0U));
            if(++bit_count==9U){perform_command();}
            break;
        case READ_WRITE_DATA:
            data=(uint8_t)((data<<1)|(input?1U:0U));
            if(++bit_count==8U) {
                if(write_enabled) {
                    if(command==1) storage()[address&511U]=data;
                    else if(command==0) memset(storage(),data,capacity); // WRAL
                }
                finish();
            }
            break;
        case SHIFT_READ:
            out=(data&0x80U)?1:0;
            data=(uint8_t)(data<<1);
            if(++bit_count==8U){bit_count=0;address=(uint16_t)((address+1U)&511U);data=storage()[address];}
            break;
        default: break;
        }
    }
    // The host SRAM is authoritative when bound. Never overwrite a loaded
    // battery save with the empty internal buffer while binding.
    bool bind_persistence(uint8_t *backing, unsigned length) {
        if(!backing || length < capacity) return false;
        external_storage = backing;
        return true;
    }
    void unbind_persistence() {
        if(external_storage) { memcpy(mem,external_storage,capacity); external_storage=0; }
    }
    bool external_persistence_bound() const {return external_storage!=0;}
    bool output() const {return out!=0;}
    uint8_t peek(unsigned index) const {return storage()[index&511U];}
    void write_persistent(unsigned index,uint8_t value){storage()[index&511U]=value;}
    const uint8_t* persistence() const {return storage();}
    void load_persistence(const uint8_t in[capacity]) {memcpy(storage(),in,capacity);}
    // Endian-stable serial protocol snapshot, EEPROM payload saved separately.
    void save(uint8_t b[snapshot_size]) const {
        b[0]=(uint8_t)address;b[1]=(uint8_t)(address>>8);
        b[2]=command;b[3]=state;b[4]=bit_count;b[5]=data;
        b[6]=last_clock;b[7]=cs_active;b[8]=write_enabled;b[9]=out;
        b[10]=0;b[11]=0;
    }
    void load(const uint8_t b[snapshot_size]) {
        address=(uint16_t)(b[0]|(unsigned(b[1])<<8));address&=511U;
        command=b[2]&3U;state=(b[3]<=FINISHED)?b[3]:(uint8_t)WAIT_START;
        bit_count=(uint8_t)(b[4]&15U);data=b[5];
        last_clock=b[6]&1U;cs_active=b[7]&1U;
        write_enabled=b[8]&1U;out=b[9]&1U;
    }
};

class AuroraMapper558Stage {
public:
    enum { snapshot_size=19 }; // register4 + flags3 + serial protocol12
    struct Mapping {unsigned prg32,chr4[2];bool wram_window;};
private:
    uint8_t regs[4];
    uint8_t eeprom_present, latch, previous_pa13;
    Aurora93C66Stage chip;
public:
    explicit AuroraMapper558Stage(bool has_eeprom=false) {
        eeprom_present=(uint8_t)has_eeprom;reset();
    }
    void reset() { // preserve EEPROM contents and EWEN state on soft reset
        memset(regs,0,sizeof regs);latch=0;previous_pa13=0;
    }
    bool write(unsigned addr,unsigned value,unsigned prg_rom_bytes) {
        addr&=65535U;value&=255U;
        if(addr<0x5000U||addr>=0x5800U)return false;
        const unsigned base=addr&0xff00U;
        if(base!=0x5000U && base!=0x5100U &&
           base!=0x5200U && base!=0x5300U)return false;
        const unsigned reg=(base>>8)&3U;
        unsigned v=value;
        // Data lines D0/D1 physically crossed, with the documented 1MiB exception.
        // $5300 writes do NOT swap D0/D1; only $5000-$5200 do.
        if(reg!=3U && !(regs[3]&2U) && !(reg==2U && prg_rom_bytes==1024U*1024U))
            v=(v&~3U)|((v>>1)&1U)|((v<<1)&2U);
        regs[reg]=(uint8_t)v;
        if(reg==2U && eeprom_present)
            chip.signal((v&4U)!=0,(v&2U)!=0,(v&1U)!=0);
        return true;
    }
    bool read(unsigned addr,unsigned &data) const {
        addr&=65535U;
        if(addr<0x5000U || addr>=0x5800U)return false;
        data=eeprom_present?(chip.output()?4U:0U):(regs[2]&4U);
        return true;
    }
    // Called for EVERY physical PPU bus fetch, not once per tile or scanline.
    // Hardware PA09 is sampled when PA13 rises; no invented scanline threshold.
    void ppu_fetch(unsigned addr) {
        addr&=0x3fffU;
        unsigned pa13=(addr>>13)&1U;
        if(!previous_pa13 && pa13 && (regs[0]&0x80U))
            latch=(uint8_t)((addr>>9)&1U);
        previous_pa13=(uint8_t)pa13;
    }
    void map(Mapping& out) const {
        out.prg32=(unsigned(regs[1])<<4)|(regs[0]&15U)|((regs[3]&4U)?0U:3U);
        out.chr4[0]=out.chr4[1]=(regs[0]&0x80U)?latch:0U;
        out.wram_window=true;
    }
    void configure_eeprom(bool present) {
        const uint8_t requested=(uint8_t)(present!=0);
        if(eeprom_present!=requested) {
            eeprom_present=requested;
            chip.reset_protocol();
        }
    }
    bool has_eeprom() const {return eeprom_present!=0;}
    bool bind_eeprom_storage(uint8_t *backing, unsigned length) {
        return chip.bind_persistence(backing,length);
    }
    void unbind_eeprom_storage() {chip.unbind_persistence();}
    Aurora93C66Stage& eeprom() {return chip;}
    const Aurora93C66Stage& eeprom() const {return chip;}
    void save(uint8_t out[snapshot_size]) const {
        memcpy(out,regs,4);out[4]=eeprom_present;out[5]=latch;
        out[6]=previous_pa13;chip.save(out+7);
    }
    void load(const uint8_t in[snapshot_size]) {
        memcpy(regs,in,4);eeprom_present=in[4]&1U;
        latch=in[5]&1U;previous_pa13=in[6]&1U;
        chip.load(in+7);
    }
};