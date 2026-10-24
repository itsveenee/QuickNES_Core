#ifndef AURORA_VN61_FDS_SOUND_STAGING_HPP
#define AURORA_VN61_FDS_SOUND_STAGING_HPP
/* Aurora vN61 — FDS expansion sound, independent C++98 behavioral model.
 * STAGING ONLY. Not wired into QuickNES/APU or enabled in any production
 * mapper. Algorithmic reference: negativeExponent/libretro-fceumm_next,
 * src/mappers/sound/fdssound.c and fdssound.h (GPL-2.0-or-later).
 * Uses fixed HQ half-CPU cycle stepping for deterministic ASIC test traces;
 * produces a pre-mixer integer, NOT 48kHz output or an audio stream.
 * State is endian-stable. No wall-clock, global state or dynamic allocation.
 */
#include <stdint.h>
#include <string.h>

class AuroraFDSSoundStage {
public:
    enum { state_size = 14 + 64 + 32 * 4 + 36 };  // version 1, exact byte serialization
private:
    struct Env {
        uint8_t speed, volume, control;
        int32_t counter;
    } env_[2];
    uint8_t wave_[64];
    int32_t mod_wave_[32];
    uint16_t wave_freq_, mod_freq_;
    uint32_t wave_pos_, mod_pos_, sweep_bias_;
    int32_t mod_output_, env_divider_, sample_cache_;
    uint8_t wave_ctrl_, mod_disabled_, master_ctrl_, master_speed_;
    static int32_t sign11(uint32_t x) {
        x &= 2047U;
        return x >= 1024U ? (int32_t)x - 2048 : (int32_t)x;
    }
    static void env_step(Env& e) {
        if (e.control & 128U) return;
        if (--e.counter <= 0) {
            e.counter = (int32_t)e.speed + 1;
            if (e.control & 64U) { if(e.volume<32U)++e.volume; }
            else if(e.volume) --e.volume;
        }
    }
    void update_mod_output() {
        int32_t k = env_[1].volume > 32U ? 32 : env_[1].volume;
        int32_t val = sign11(sweep_bias_) * k;
        if (val & 0xf0) {
            val /= 256;
            if (sweep_bias_ & 0x400U) --val;
            else val += 2;
        } else val /= 256;
        if (val >= 194) val -= 258;
        if (val < -64) val += 256;
        mod_output_ = val;
    }
    void clock_units() {
        if (!mod_disabled_) {
            const uint32_t old = mod_pos_;
            mod_pos_ += mod_freq_;
            const uint32_t mask = 0x3fU << 12;
            if ((old & mask) != (mod_pos_ & mask)) {
                int32_t delta=mod_wave_[(mod_pos_ >> 17)&31U];
                sweep_bias_=(sweep_bias_ + (uint32_t)delta)&2047U;
                if(delta==16) sweep_bias_=0;
            }
            update_mod_output();
        }
        if (!(master_ctrl_ & 128U) && !(wave_ctrl_ & 128U)) {
            int32_t rate = (int32_t)wave_freq_ * 64;
            if(!mod_disabled_) rate += (int32_t)wave_freq_ * mod_output_;
            if(rate<0) rate=0;
            wave_pos_ += (uint32_t)rate;
        }
        if(master_speed_) {
            if(--env_divider_ <= 0) {
                env_divider_ += (int32_t)master_speed_ * 8;
                if(!(wave_ctrl_ & 64U)) {env_step(env_[0]);env_step(env_[1]);}
            }
        }
    }
    static void put32(uint8_t*& p, uint32_t n) {
        for(unsigned i=0;i<4;++i) *p++=(uint8_t)(n>>(i*8));
    }
    static uint32_t get32(const uint8_t*& p) {
        uint32_t x=0;
        for(unsigned i=0;i<4;++i) x |= (uint32_t)(*p++)<<(i*8);
        return x;
    }
public:
    AuroraFDSSoundStage() {reset();}
    void reset() {
        memset(env_,0,sizeof env_);
        memset(wave_,0,sizeof wave_);
        memset(mod_wave_,0,sizeof mod_wave_);
        wave_freq_=mod_freq_=0;wave_pos_=mod_pos_=sweep_bias_=0;
        mod_output_=env_divider_=sample_cache_=0;
        wave_ctrl_=mod_disabled_=master_ctrl_=master_speed_=0;
        // FCEUmm FDS BIOS-like audio startup register sequence.
        write(0x4080,0x80);write(0x408a,0xe8);
        write(0x4082,0);write(0x4083,0x80);
        write(0x4084,0x80);write(0x4085,0);
        write(0x4086,0);write(0x4087,0x80);
        write(0x4089,0);
    }
    bool write(unsigned a, unsigned value) {
        const unsigned v=value&255U;
        if(a>=0x4040U && a<=0x407fU) {
            if(master_ctrl_&128U)wave_[a&63U]=(uint8_t)(v&63U);
            return true;
        }
        switch(a) {
        case 0x4080: case 0x4084: {
            Env& e=env_[a==0x4080?0:1];
            e.speed=(uint8_t)(v&63U);e.control=(uint8_t)(v&0xc0U);
            e.counter=e.speed+1;
            if(e.control&128U)e.volume=e.speed;
            return true;
        }
        case 0x4082: wave_freq_=(uint16_t)((wave_freq_&0x0f00U)|v);return true;
        case 0x4083:
            wave_freq_=(uint16_t)((wave_freq_&255U)|((v&15U)<<8));
            wave_ctrl_=(uint8_t)(v&0xc0U);
            if(wave_ctrl_&128U)wave_pos_=0;
            return true;
        case 0x4085:sweep_bias_=(v&127U)<<4;mod_pos_=0;return true;
        case 0x4086:mod_freq_=(uint16_t)((mod_freq_&0x0f00U)|v);return true;
        case 0x4087:
            mod_freq_=(uint16_t)((mod_freq_&255U)|((v&15U)<<8));
            mod_disabled_=(uint8_t)((v&128U)!=0);
            if(mod_disabled_)mod_pos_=0;
            return true;
        case 0x4088:
            if(mod_disabled_) {
                for(unsigned i=0;i<31;++i)mod_wave_[i]=mod_wave_[i+1];
                static const int8_t shifts[8]={0,1,2,4,16,-4,-2,-1};
                mod_wave_[31]=shifts[v&7U];
            }
            return true;
        case 0x4089:master_ctrl_=(uint8_t)v;return true;
        case 0x408a:master_speed_=(uint8_t)v;return true;
        default:return false;
        }
    }
    bool read(unsigned a, unsigned& value) const {
        if(a>=0x4040U && a<=0x407fU) {
            value=64U|wave_[(master_ctrl_&128U)?(a&63U):((wave_pos_>>22)&63U)];
            return true;
        }
        if(a==0x4090U){value=64U|env_[0].volume;return true;}
        if(a==0x4092U){value=64U|env_[1].volume;return true;}
        return false;
    }
    // Two calls correspond to one CPU cycle in FCEUmm's HQ model.
    // Delivers pre-mixer unscaled amplitude; host mixer is NOT modeled.
    int32_t tick_half_cpu() {
        const uint32_t old=wave_pos_;
        clock_units();
        if((old^wave_pos_)&(1U<<22)) {
            int32_t vol = env_[0].volume > 32 ? 32 : env_[0].volume;
            int32_t raw=(int32_t)wave_[(wave_pos_>>22)&63U]*vol*4;
            static const uint32_t reciprocal[4]={32768U,21845U,16384U,13107U};
            sample_cache_=(int32_t)(((uint32_t)raw*reciprocal[master_ctrl_&3U])>>16);
        }
        return sample_cache_;
    }
    int32_t current_sample() const {return sample_cache_;}
    unsigned wave_position() const {return wave_pos_;}
    unsigned modulation_position() const {return mod_pos_;}
    unsigned envelope_volume(unsigned i) const {return env_[i&1U].volume;}
    unsigned wave_byte(unsigned i) const {return wave_[i&63U];}
    unsigned master_control() const {return master_ctrl_;}
    void save(uint8_t out[state_size])const {
        uint8_t* p=out;
        for(unsigned i=0;i<2;++i) {
            *p++=env_[i].speed;*p++=env_[i].volume;*p++=env_[i].control;
            put32(p,(uint32_t)env_[i].counter);
        }
        memcpy(p,wave_,64);p+=64;
        for(unsigned i=0;i<32;++i)put32(p,(uint32_t)mod_wave_[i]);
        put32(p,wave_freq_);put32(p,mod_freq_);put32(p,wave_pos_);
        put32(p,mod_pos_);put32(p,sweep_bias_);put32(p,(uint32_t)mod_output_);
        put32(p,(uint32_t)env_divider_);put32(p,(uint32_t)sample_cache_);
        *p++=wave_ctrl_;*p++=mod_disabled_;*p++=master_ctrl_;*p++=master_speed_;
        // Explicitly zero reserved padding, with compile-time size assertion.
        while(p<out+state_size)*p++=0;
    }
    bool load(const uint8_t in[state_size]) {
        const uint8_t* p=in;
        Env temp[2];
        for(unsigned i=0;i<2;++i) {
            temp[i].speed=*p++;temp[i].volume=*p++;temp[i].control=*p++;
            temp[i].counter=(int32_t)get32(p);
        }
        if(temp[0].volume>63U||temp[1].volume>63U)return false;
        uint8_t waves[64];memcpy(waves,p,64);p+=64;
        for(unsigned i=0;i<64;++i)if(waves[i]>63U)return false;
        int32_t mods[32];for(unsigned i=0;i<32;++i)mods[i]=(int32_t)get32(p);
        uint32_t wfreq=get32(p),mfreq=get32(p),wpos=get32(p),mpos=get32(p);
        uint32_t bias=get32(p),mo=get32(p),divider=get32(p),cache=get32(p);
        uint8_t wc=*p++,md=*p++,mc=*p++,ms=*p++;
        if(wfreq>4095U||mfreq>4095U||bias>2047U||md>1U)return false;
        for(unsigned i=0;i<2;++i)env_[i]=temp[i];
        memcpy(wave_,waves,64);memcpy(mod_wave_,mods,sizeof mods);
        wave_freq_=(uint16_t)wfreq;mod_freq_=(uint16_t)mfreq;
        wave_pos_=wpos;mod_pos_=mpos;sweep_bias_=bias;
        mod_output_=(int32_t)mo;env_divider_=(int32_t)divider;sample_cache_=(int32_t)cache;
        wave_ctrl_=wc;mod_disabled_=md;master_ctrl_=mc;master_speed_=ms;
        return true;
    }
};
// Fails at compile time if serializer-size constant is smaller than payload.
typedef char AuroraFDSStateSizeCheck[(AuroraFDSSoundStage::state_size >= 14+64+128+36) ? 1 : -1];
#endif