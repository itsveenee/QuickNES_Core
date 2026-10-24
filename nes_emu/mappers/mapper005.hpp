#pragma once

// NES MMC5 (mapper 5)
//
// Historically this core implemented only enough of the MMC5 to run
// Castlevania 3 (U). This version fleshes it out to cover the rest of the
// commercial library and common homebrew: the four PRG banking modes, the
// four CHR banking modes with the upper-bit register, PRG-RAM (work/save RAM)
// at $6000-$DFFF with the write-protect handshake, the hardware multiplier,
// nametable-mode selection, and the scanline IRQ.
//
// Some MMC5 features cannot be reproduced faithfully in this core because its
// PPU exposes no per-tile fetch hook: extended-attribute mode (ExGrafix), the
// vertical split window, and a separate 8x16-sprite CHR bank set. Games that
// lean on those for cosmetic effects still run, but those specific effects are
// approximated or absent. The MMC5's two extra pulse channels and PCM are not
// emulated. PRG-RAM bank windows at $8000-$DFFF are implemented
// using the core's 32 KiB SRAM backing, with write-protect enforced.

// Nes_Emu 0.7.0. http://www.slack.net/~ant/

#include "Nes_Mapper.h"

#include "Nes_Core.h"
#include "Blip_Buffer.h"
#include <string.h>

/* Copyright (C) 2004-2006 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for
more details. You should have received a copy of the GNU Lesser General
Public License along with this module; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */

#include "blargg_source.h"


// Registered (savestated) MMC5 state. All bytes, so no padding; kept well
// under max_mapper_state_size (512). ExRAM is deliberately NOT part of this
// struct: at 1 KiB it would blow the state budget, and the games that read it
// back are the ExGrafix titles this core can't fully render regardless.
struct mmc5_state_t
{
	uint8_t prg_mode;         // $5100 & 3
	uint8_t chr_mode;         // $5101 & 3
	uint8_t ram_protect1;     // $5102 & 3
	uint8_t ram_protect2;     // $5103 & 3
	uint8_t exram_mode;       // $5104 & 3
	uint8_t nametable_mode;   // $5105
	uint8_t fill_tile;        // $5106
	uint8_t fill_attr;        // $5107 & 3
	uint8_t prg_banks [5];    // $5113-$5117
	uint8_t chr_banks [12];   // $5120-$512B
	uint8_t chr_upper;        // $5130 & 3
	uint8_t mul_a;            // $5205
	uint8_t mul_b;            // $5206
	uint8_t irq_enabled;      // $5204
	uint8_t irq_scanline;     // $5203
	// AURORA_CURSED_FIVE_MMC5_AUDIT_VN91: extension AFTER legacy 30 bytes.
	// Bits captured when a CHR bank register is written, not when $5130 changes.
	uint8_t chr_bank_high[12];
	// vN206: compact expansion sound state (legacy states <=42 bytes).
	uint8_t pulse_reg[8];
	uint8_t pulse_len[2], pulse_phase[2], env_div[2], env_level[2];
	uint8_t pulse_gate, pcm_ctrl, pcm_value, pcm_irq, quarter_clock;
	uint8_t split_ctrl,split_scroll,split_bank;
};
BOOST_STATIC_ASSERT( sizeof (mmc5_state_t) == 66 );

// MMC5

/* AURORA_M79_MMC5_ACCURACY_VN207: vN203-207 MMC5 CHR bank separation / expansion sound / IRQ. */
/* AURORA_MMC5_VN215_NO_SRAM_STATE */
class Mapper005 : public Nes_Mapper, mmc5_state_t {
public:
	Mapper005() : audio_out_(NULL), audio_cursor_(0), next_quarter_(7457), audio_level_(0)
	{
		for(int i=0;i<2;++i) next_pulse_[i]=0;
		// vN206: additive expansion mix; NESdev specifies inverted polarity.
		audio_synth_.volume(BLIP_Q30_ONE / 256);
		mmc5_state_t* state = this;
		register_state( state, sizeof *state );
	}

	// Savestates must preserve all four PRG-RAM banks, not only the
	// 8 KiB window currently selected at $6000.
	virtual long aurora_extra_sram_save_size() const { return 0x20000L; }

	// Dedicated ExRAM savestate, preserving old MAPR (<512B) snapshots.
	virtual bool aurora_save_exram(uint8_t* dest,unsigned n) const {
		if(n!=0x400 || !dest)return false;
		memcpy(dest,exram,0x400);return true;
	}
	virtual void aurora_load_exram(const uint8_t* src,unsigned n) {
		if(n!=0x400 || !src)return;
		memcpy(exram,src,0x400);
		sync_mirror();sync_exgrafix();sync_split();
	}

	virtual void reset_state()
	{
		irq_time = no_irq;

		prg_mode = 3;          // power-on: four 8K banks, last fixed
		chr_mode = 0;
		ram_protect1 = 0;
		ram_protect2 = 0;
		/* MMC5 ExRAM power-on mode is %11 (CPU read-only). */
		exram_mode = 3;
		nametable_mode = 0;
		fill_tile = 0;
		fill_attr = 0;
		chr_upper = 0;
		mul_a = mul_b = 0xff;
		irq_enabled = 0;
		irq_scanline = 0;

		prg_banks [0] = 0;      // $5113
		prg_banks [1] = 0x80;   // $5114 (ROM)
		prg_banks [2] = 0x80;   // $5115 (ROM)
		prg_banks [3] = 0x80;   // $5116 (ROM)
		prg_banks [4] = 0x7f;   // $5117 (ROM, last bank)

		for ( int i = 0; i < 12; i++ )
		{ chr_banks [i] = 0; chr_bank_high[i] = 0; }

		memset( exram, 0, sizeof exram );
		memset( fill_nt, 0, sizeof fill_nt );
		memset( empty_nt, 0, sizeof empty_nt );
		memset(pulse_reg,0,sizeof pulse_reg);
		memset(pulse_len,0,sizeof pulse_len);
		memset(pulse_phase,0,sizeof pulse_phase);
		memset(env_div,0,sizeof env_div);
		memset(env_level,0,sizeof env_level);
		pulse_gate=pcm_ctrl=pcm_irq=quarter_clock=0;
		pcm_value=0;
		split_ctrl=split_scroll=split_bank=0;
		audio_cursor_=0;
		audio_level_=0;
		next_quarter_=7457;
		for(int i=0;i<2;++i)next_pulse_[i]=0;
	}

	virtual void read_state( mapper_state_t const& in )
	{
		Nes_Mapper::read_state( in );
		// Legacy MMC5 states were exactly 30 bytes, with only a global
		// $5130. Reconstruct the old effective mapping for compatibility.
		if ( in.size <= 30 )
		{
			for ( int i = 0; i < 12; i++ )
				chr_bank_high[i] = (uint8_t)((chr_mode == 3) ?
					(chr_upper & 3) : ((chr_mode == 2) ? (chr_upper & 1) : 0));
			sync_chr();
		}
		irq_time = no_irq;
		if(in.size<=42) {
			memset(pulse_reg,0,sizeof pulse_reg);
			memset(pulse_len,0,sizeof pulse_len);
			memset(pulse_phase,0,sizeof pulse_phase);
			memset(env_div,0,sizeof env_div);
			memset(env_level,0,sizeof env_level);
			pulse_gate=pcm_ctrl=pcm_value=pcm_irq=quarter_clock=0;
		}
		if(in.size<=63)split_ctrl=split_scroll=split_bank=0;
		audio_cursor_=0;audio_level_=0;next_quarter_=7457;
		for(int i=0;i<2;++i)next_pulse_[i]=0;
	}

	enum { regs_addr = 0x5100 };

	// vN206: cartridge audio: two square channels (no sweep) and PCM DAC.
	// Sound runs only when mapper 5 is active. Timing uses CPU clocks;
	// envelope/length cadence is approximated, not yet MMC5 silicon-exact.
	virtual int channel_count() const { return 1; }
	virtual void set_channel_buf(int index,Blip_Buffer* b) {
		if(index!=0 || audio_out_==b)return;
		audio_out_=b;audio_level_=0;
	}
	virtual void set_treble(blip_eq_t const& eq) { audio_synth_.treble_eq(eq); }
	static int pulse_length_table(int v) {
		static const uint8_t t[32]={10,254,20,2,40,4,80,6,160,8,60,10,14,12,26,14,
			12,16,24,18,48,20,96,22,192,24,72,26,16,28,32,30};
		return t[v&31];
	}
	int audio_mix() const {
		static const uint8_t duty[4]={0x02,0x06,0x1e,0xf9};
		int n=-(int)pcm_value;
		for(int i=0;i<2;++i)if((pulse_gate&(1<<i)) && pulse_len[i]) {
			const int control=pulse_reg[i*4];
			const unsigned phase=(unsigned)pulse_phase[i]&7;
			if(duty[(control>>6)&3] & (1u<<phase))
				n -= (control & 0x10) ? (control&15) : env_level[i];
		}
		return n;
	}
	void audio_emit(nes_time_t when) {
		const int level=audio_mix(),delta=level-audio_level_;
		if(delta && audio_out_) audio_synth_.offset(when,delta,audio_out_);
		audio_level_=level;
	}
	void audio_run(nes_time_t until) {
		if(until<=audio_cursor_)return;
		// Skip CPU-clock oscillator ticks when pulse channel is muted.
		for(int i=0;i<2;++i)
			if(!(pulse_gate&(1<<i)) || !pulse_len[i])next_pulse_[i]=until+1;
		// Advance active oscillators and common quarter-frame clock.
		while(true) {
			nes_time_t event=next_quarter_;
			for(int i=0;i<2;++i)if(next_pulse_[i]<event)event=next_pulse_[i];
			if(event>until)break;
			if(event<audio_cursor_)event=audio_cursor_;
			for(int i=0;i<2;++i)if(next_pulse_[i]<=event) {
				const int timer=pulse_reg[4*i+2]|((pulse_reg[4*i+3]&7)<<8);
				const nes_time_t period=2L*(timer+1);
				next_pulse_[i]=event+period;
				pulse_phase[i]=(pulse_phase[i]+1)&7;
			}
			if(next_quarter_<=event){
				next_quarter_=event+7457;
				for(int i=0;i<2;++i){
					const uint8_t ctl=pulse_reg[i*4];
					if(!env_div[i]) {env_div[i]=ctl&15;
						if(env_level[i])--env_level[i];
						else if(ctl&0x20)env_level[i]=15;
					}else --env_div[i];
					if((quarter_clock&1) && !(ctl&0x20) && pulse_len[i])--pulse_len[i];
				}
				++quarter_clock;
			}
			audio_emit(event);
			audio_cursor_=event;
		}
		audio_cursor_=until;
	}
	bool audio_write(nes_time_t t,nes_addr_t addr,int data) {
		if((addr>=0x5000 && addr<=0x5007) || addr==0x5010 || addr==0x5011 || addr==0x5015) {
			audio_run(t);
			if(addr<=0x5007){
				const int i=(addr>>2)&1,r=addr&3;
				pulse_reg[4*i+r]=(uint8_t)data;
				if(r==3){if(pulse_gate&(1<<i))pulse_len[i]=(uint8_t)pulse_length_table(data>>3);
						pulse_phase[i]=0;env_level[i]=15;env_div[i]=pulse_reg[4*i]&15;
						next_pulse_[i]=t+2L*((pulse_reg[4*i+2]|((pulse_reg[4*i+3]&7)<<8))+1);}
			}else if(addr==0x5010){pcm_ctrl=(uint8_t)data;
				emu().mapper_pcm_read_intercept((pcm_ctrl&1)!=0);irq_changed();}
			else if(addr==0x5011 && !(pcm_ctrl&1)){
				if(!data){pcm_irq=1;irq_changed();}
				else {pcm_irq=0;pcm_value=(uint8_t)data;irq_changed();}
			}else if(addr==0x5015){const int was=pulse_gate;pulse_gate=(uint8_t)(data&3);
				for(int i=0;i<2;++i){
					if(!(pulse_gate&(1<<i)))pulse_len[i]=0;
					else if(!(was&(1<<i)))next_pulse_[i]=t+2L*((pulse_reg[4*i+2]|((pulse_reg[4*i+3]&7)<<8))+1);
				}}
			audio_emit(t);
			return true;
		}
		return false;
	}
	int audio_read(nes_time_t t,nes_addr_t addr){
		if(addr==0x5010){audio_run(t);int val=(pcm_irq && (pcm_ctrl&0x80))?0x80:0;
			pcm_irq=0;irq_changed();return val|1;}
		if(addr==0x5015){audio_run(t);return (pulse_len[0]?1:0)|(pulse_len[1]?2:0);}
		if((pcm_ctrl&1) && addr>=0x8000 && addr<0xc000){
			const int val=emu().mapper_prg_peek(addr);
			audio_run(t);
			if(!val){pcm_irq=1;irq_changed();}
			else {pcm_value=(uint8_t)val;pcm_irq=0;irq_changed();}
			audio_emit(t);return val;
		}
		return -1;
	}

	// CPU time at which the scanline IRQ counter reaches compare scanline 'sl'.
	// Returns no_irq for compares that never occur within visible rendering.
	nes_time_t irq_compare_time( int sl ) const
	{
		if ( sl && sl < 240 )
			return (341 * 21 + 128 + (sl * 341)) / 3;
		return no_irq;
	}

	virtual nes_time_t next_irq( nes_time_t )
	{
		if (pcm_irq && (pcm_ctrl & 0x80)) return 0;
		if ( irq_enabled & 0x80 ) return irq_time;
		return no_irq;
	}

	// Re-arm the scanline IRQ for the next frame. The MMC5 counter runs every
	// frame and re-asserts at the compare scanline; a $5204 read acknowledges
	// (de-asserts) the current assertion but does not stop the counter. Games
	// that reprogram $5203 each frame (Castlevania 3) overwrite this anyway, so
	// this only changes behaviour for games that arm once and rely on the
	// counter re-firing (Metal Slader Glory's message-window raster split).
	virtual void end_frame( nes_time_t length )
	{
		audio_run(length);
		audio_cursor_-=length;next_quarter_-=length;
		for(int i=0;i<2;++i)next_pulse_[i]-=length;
		irq_time = irq_compare_time( irq_scanline );
		irq_changed();
	}

	// -- PRG banking ---------------------------------------------------------

	// A $5114-$5117 value: bit 7 set selects ROM, clear selects PRG-RAM.
	// $5117 is always ROM; $5114-$5116 can also select PRG-RAM.
	/* AURORA_MMC5_32K_PRGRAM_VN201
	 * PRG-RAM fisica: 32 KiB em 4 bancos de 8 KiB. Mantem CPU fetch,
	 * leitura de dados e escrita no mesmo backing. Para janelas de 16 KiB,
	 * o bit 0 do registrador nao endereca uma meia janela diferente.
	 * Acesso ao banco da SRAM em $6000 e controlado por $5113.
	 */
	void set_prg_reg( int slot_addr, bank_size_t size, int value )
	{
		if ( value & 0x80 ) // ROM
		{
			set_prg_bank( slot_addr, size, (value & 0x7f) >> (size - bank_8k) );
		}
		else // PRG-RAM, $8000-$DFFF
		{
			const unsigned bytes = 1u << size;
			const unsigned bank = (unsigned)(value & 15) & ~((bytes / 0x2000u) - 1u);
			map_sram_mirror( slot_addr, bytes, bank * 0x2000u, bytes );
		}
	}

	// RAM selected in upper PRG space, using the CURRENT MMC5 PRG mode.
	// This is needed because writes to $8000+ go via Mapper::write(),
	// whereas instruction fetch/data reads use map_sram_mirror().
	int upper_ram_bank( nes_addr_t addr ) const
	{
		if ( addr < 0x8000 || addr >= 0xe000 || prg_mode == 0 ) return -1;
		const int slot = (addr - 0x8000) >> 13;
		int reg = 0;
		int bank = 0;
		if ( prg_mode == 1 || (prg_mode == 2 && slot < 2) )
		{
			if ( prg_mode == 1 && slot >= 2 ) return -1; // $C000 fixed ROM
			reg = 2; // $5115: one 16 KiB window
			bank = (prg_banks [reg] & 14) | (slot & 1);
		}
		else if ( prg_mode == 2 )
		{
			if ( slot == 3 ) return -1; // $E000 fixed ROM
			reg = 3; // $5116: 8 KiB at $C000
			bank = prg_banks [reg] & 15;
		}
		else // mode 3: three independently selected 8 KiB windows
		{
			reg = 1 + slot;
			bank = prg_banks [reg] & 15;
		}
		return (prg_banks [reg] & 0x80) ? -1 : bank;
	}

	void sync_prg()
	{
		// $6000 window is always PRG-RAM (32 KiB total, banked in 8 KiB).
		enable_sram( true, ram_write_protected() );
		// $5113 selects one of four physical 8 KiB banks for $6000-$7FFF.
		// This was missing in vN200 and caused aliasing of all 32 KiB.
		set_sram_bank( prg_banks [0] & 15 );

		switch ( prg_mode )
		{
		case 0: // one 32K ROM bank from $5117
			set_prg_bank( 0x8000, bank_32k, (prg_banks [4] & 0x7f) >> 2 );
			break;

		case 1: // two 16K: $5115 at $8000, $5117 at $C000
			set_prg_reg( 0x8000, bank_16k, prg_banks [2] );
			set_prg_bank( 0xC000, bank_16k, (prg_banks [4] & 0x7f) >> 1 );
			break;

		case 2: // 16K $5115 @ $8000, 8K $5116 @ $C000, 8K $5117 @ $E000
			set_prg_reg( 0x8000, bank_16k, prg_banks [2] );
			set_prg_reg( 0xC000, bank_8k,  prg_banks [3] );
			set_prg_bank( 0xE000, bank_8k, prg_banks [4] & 0x7f );
			break;

		case 3: // four 8K: $5114 $5115 $5116 $5117
			set_prg_reg( 0x8000, bank_8k, prg_banks [1] );
			set_prg_reg( 0xA000, bank_8k, prg_banks [2] );
			set_prg_reg( 0xC000, bank_8k, prg_banks [3] );
			set_prg_bank( 0xE000, bank_8k, prg_banks [4] & 0x7f );
			break;
		}
	}

	// -- CHR banking (background set) ----------------------------------------

	void sync_chr()
	{
		// Sprite registers: $5120-$5127, in all CHR modes.
		// $5130 is LATCHED on each write; changing it later cannot remap them.
		if ( chr_mode == 0 )
			set_chr_bank(0x0000,bank_8k,chr_banks[7]);
		else if (chr_mode == 1)
		{
			set_chr_bank(0x0000,bank_4k,chr_banks[3]);
			set_chr_bank(0x1000,bank_4k,chr_banks[7]);
		}
		else if (chr_mode == 2)
		{
			for (int i=0;i<4;++i)
				set_chr_bank(i*0x800,bank_2k,chr_banks[1+i*2] |
					(chr_bank_high[1+i*2]<<8));
		}
		else
		{
			for (int i=0;i<8;++i)
				set_chr_bank(i*0x400,bank_1k,chr_banks[i] | (chr_bank_high[i]<<8));
		}

		// vN204: on MMC5 with 8x16 sprites the background fetches use
		// $5128-$512B independently of sprite banks $5120-$5127.
		// When 8x8 sprites are selected, PPU uses the normal sprite bank set.
		// $5128-$512B are mirrored as described in NESdev MMC5 tables.
		if (chr_mode == 0)
			emu().ppu.set_mmc5_bg_chr_bank(0,0x2000,chr_banks[11]);
		else if (chr_mode == 1)
		{
			emu().ppu.set_mmc5_bg_chr_bank(0,0x1000,chr_banks[11]);
			emu().ppu.set_mmc5_bg_chr_bank(0x1000,0x1000,chr_banks[11]);
		}
		else if (chr_mode == 2)
		{
			for(int j=0;j<2;++j)
			{
				emu().ppu.set_mmc5_bg_chr_bank(j*0x1000,0x800,chr_banks[9]|(chr_bank_high[9]<<8));
				emu().ppu.set_mmc5_bg_chr_bank(j*0x1000+0x800,0x800,chr_banks[11]|(chr_bank_high[11]<<8));
			}
		}
		else
		{
			for(int j=0;j<2;++j)
				for(int i=0;i<4;++i)
					emu().ppu.set_mmc5_bg_chr_bank(j*0x1000+i*0x400,0x400,
						chr_banks[8+i]|(chr_bank_high[8+i]<<8));
		}
		emu().ppu.set_mmc5_bg_enabled(true);
	}

	// Enable ExGrafix (extended-attribute) background rendering in the PPU when
	// the MMC5 is in ExRAM mode 1; otherwise leave normal rendering in force.
	// In mode 1 each background tile takes its 4 KiB CHR bank and palette from
	// the per-tile ExRAM byte, which the PPU can only honour through this hook.
	void sync_exgrafix()
	{
		if ( exram_mode == 1 )
			set_exgrafix( exram, chr_upper );
		else
			set_exgrafix( NULL, 0 );
	}

	// vN211-212: PPU background horizontal split, enabled only with ExRAM
	// mode 0/1 and background rendering. Non-split uses normal nt/ex-attr.
	void sync_split()
	{
		emu().ppu.render_bg_until(emu().clock());
		emu().ppu.set_mmc5_split((split_ctrl&0x80)!=0 && exram_mode<2,
			(split_ctrl&0x40)!=0,split_ctrl&0x1f,split_scroll,split_bank,exram);
	}

	// -- Nametables / mirroring ---------------------------------------------

/* vN175: MMC5 selects CIRAM0, CIRAM1, ExRAM or fill per 1 KiB NT slot.
	 * Keep non-MMC5 mirroring unchanged; mapped pointer remains stable. */
	void sync_mirror()
	{
		int p[4];
		for (int i=0;i<4;i++)
			p[i] = ((nametable_mode >> (i*2)) & 3) == 1 ? 1 : 0;
		mirror_manual(p[0],p[1],p[2],p[3]);
		for (int i=0;i<4;i++)
		{
			int sel = (nametable_mode >> (i*2)) & 3;
			if (sel == 2)
			{
				if (exram_mode < 2)
					emu().ppu.set_nt_bank_external(i,exram,true);
				else
					emu().ppu.set_nt_bank_external(i,empty_nt,false);
			}
			else if (sel == 3)
				emu().ppu.set_nt_bank_external(i,fill_nt,false);
		}
	}
	void sync_fill()
	{
		memset(fill_nt,fill_tile,0x3c0);
		const uint8_t attr=(uint8_t)((fill_attr & 3)*0x55);
		memset(fill_nt+0x3c0,attr,0x40);
	}
	bool exram_during_render(nes_time_t t) const
	{
		const nes_time_t first=(341*21+128)/3;
		const nes_time_t last =(341*261+128)/3;
		return ppu_enabled() && t>=first && t<last;
	}

	// -----------------------------------------------------------------------

	virtual bool write_intercepted( nes_time_t time, nes_addr_t addr, int data )
	{
		if(audio_write(time,addr,data))return true;
		unsigned reg = addr - regs_addr;
		if ( reg <= 0x30 )
		{
			switch ( reg )
			{
			case 0x00: prg_mode     = data & 3; sync_prg();   break; // $5100
			case 0x01: chr_mode     = data & 3; sync_chr();   break; // $5101
			case 0x02: ram_protect1 = data & 3; sync_prg();   break; // $5102
			case 0x03: ram_protect2 = data & 3; sync_prg();   break; // $5103
			case 0x04: exram_mode   = data & 3; sync_exgrafix(); sync_mirror(); sync_split(); break; // $5104
			case 0x05: nametable_mode = data; sync_mirror();  break; // $5105
			case 0x06: fill_tile    = data; sync_fill();          break; // $5106
			case 0x07: fill_attr    = data & 3; sync_fill();    break; // $5107

			case 0x13: prg_banks [0] = data; sync_prg(); break; // $5113
			case 0x14: prg_banks [1] = data; sync_prg(); break; // $5114
			case 0x15: prg_banks [2] = data; sync_prg(); break; // $5115
			case 0x16: prg_banks [3] = data; sync_prg(); break; // $5116
			case 0x17: prg_banks [4] = data; sync_prg(); break; // $5117

			case 0x20: case 0x21: case 0x22: case 0x23:
			case 0x24: case 0x25: case 0x26: case 0x27:
				chr_banks [reg - 0x20] = data;
				chr_bank_high[reg - 0x20] = (uint8_t)((chr_mode == 3) ?
					(chr_upper & 3) : ((chr_mode == 2) ? (chr_upper & 1) : 0));
				sync_chr();
				break;

			case 0x28: case 0x29: case 0x2a: case 0x2b:
				chr_banks [8 + (reg - 0x28)] = data;
				chr_bank_high[8 + (reg - 0x28)] = (uint8_t)((chr_mode == 3) ?
					(chr_upper & 3) : ((chr_mode == 2) ? (chr_upper & 1) : 0));
				sync_chr();
				break;

			case 0x30: chr_upper = data & 3; sync_exgrafix(); break; // $5130
			}
		}
		else if ( addr >= 0x5200 && addr <= 0x5202 )
		{
			if(addr==0x5200)split_ctrl=(uint8_t)data;
			else if(addr==0x5201)split_scroll=(uint8_t)data;
			else split_bank=(uint8_t)data;
			sync_split();
		}
		else if ( addr == 0x5203 ) // IRQ scanline compare
		{
			irq_scanline = data;
			irq_time = no_irq;
			if ( data && data < 240 )
			{
				irq_time = (341 * 21 + 128 + (data * 341)) / 3;
				if ( irq_time < time )
					irq_time = no_irq;
			}
			irq_changed();
		}
		else if ( addr == 0x5204 ) // IRQ enable
		{
			irq_enabled = data;
			irq_changed();
		}
		else if ( addr == 0x5205 ) // multiplicand
		{
			mul_a = data;
		}
		else if ( addr == 0x5206 ) // multiplier
		{
			mul_b = data;
		}
		else if ( (unsigned) (addr - 0x5c00) < 0x400 ) // ExRAM
		{
			/* vN174: ExRAM CPU access obeys $5104.  Mode 3 is RO;
			 * modes 0/1 write only in frame (blanking writes zero). */
			if (exram_mode == 2) exram[addr-0x5c00]=(uint8_t)data;
			else if (exram_mode < 2)
				exram[addr-0x5c00]=exram_during_render(time) ? (uint8_t)data : 0;
		}
		else
		{
			return false;
		}

		return true;
	}

	virtual int read( nes_time_t time, nes_addr_t addr )
	{
		const int sound_read=audio_read(time,addr);
		if(sound_read>=0)return sound_read;
		if ( addr == 0x5204 )
		{
			// MMC5 IRQ/status. bit 7 = scanline IRQ pending, bit 6 = "in
			// frame" (PPU currently rendering a visible scanline). This core has no
			// per-scanline hook, so both bits are derived from the current
			// time: the visible-render window runs from scanline 0 to 239,
			// and the IRQ is pending once that window has reached the compare
			// scanline programmed at $5203 (while enabled).
			int status = 0;

			// Mapper time is in CPU cycles; a PPU scanline is 341/3 cycles and
			// visible rendering starts after the 21 pre-render/vblank lines
			// (matching the $5203 -> irq_time conversion above).
			nes_time_t frame_start = (341 * 21 + 128) / 3;
			nes_time_t frame_end   = (341 * 21 + 128 + 240 * 341) / 3;
			if ( ppu_enabled() && time >= frame_start && time < frame_end )
				status |= 0x40; // in frame

			// Report the pending flag and acknowledge it: clearing irq_time
			// de-asserts the IRQ line for the rest of this frame (so a polling
			// game like Metal Slader Glory can proceed past its wait loop),
			// while end_frame re-arms the counter for the next frame. Games
			// that re-arm explicitly via $5203 (Castlevania 3) are unaffected.
			if ( irq_time != no_irq && time >= irq_time )
			{
				status |= 0x80;
				irq_time = no_irq;
				irq_changed();
			}

			return status;
		}
		if ( addr == 0x5205 )
			return (mul_a * mul_b) & 0xff;
		if ( addr == 0x5206 )
			return (mul_a * mul_b) >> 8;
		if ( (unsigned) (addr - 0x5c00) < 0x400 )
			return exram_mode >= 2 ? exram[addr-0x5c00] : -1; // open bus
		return -1; // not handled / open bus
	}

	void apply_mapping()
	{
		// Intercept the whole $5000-$5FFF register space so PRG-RAM control,
		// the multiplier, IRQ, and ExRAM all reach the mapper (reads too, for
		// the multiplier and ExRAM).
		intercept_writes( 0x5000, 0x1000 );
		intercept_reads( 0x5000, 0x1000 );
		// vN206 PCM read-mode: intercept enabled only in read PCM mode for $8000-$BFFF;
		// CPU opcode fetches remain mapped directly by the QuickNES CPU.
		emu().mapper_pcm_read_intercept((pcm_ctrl & 1) != 0);

		sync_prg();
		sync_chr();
		sync_fill();
		sync_mirror();
		sync_split();

		// Set ExGrafix state directly (no mid-frame render flush needed here).
		if ( exram_mode == 1 )
			emu().ppu.set_exgrafix( exram, chr_upper );
		else
			emu().ppu.set_exgrafix( NULL, 0 );
	}

	virtual void write( nes_time_t, nes_addr_t addr, int data )
	{
		if ( ram_write_protected() ) return;
		const int bank = upper_ram_bank( addr );
		if ( bank >= 0 )
			sram_data_ptr() [bank * 0x2000 + (addr & 0x1fff)] = (uint8_t)data;
	}

	nes_time_t irq_time;

private:
	uint8_t exram [0x400]; // 1 KiB internal ExRAM (dedicated M5EX savestate block)
	uint8_t fill_nt[0x400]; // hardware fill: $5106 tile and $5107 attribute
	uint8_t empty_nt[0x400]; // disabled ExRAM nametable in CPU modes
	/* vN206 mapper-only audio (not shared with core's APU). */
	Blip_Buffer* audio_out_;
	Blip_Synth<blip_good_quality,256> audio_synth_;
	nes_time_t audio_cursor_,next_pulse_[2],next_quarter_;
	int audio_level_;

	bool ram_write_protected() const
	{
		// $5102 must hold %10 and $5103 must hold %01 to allow PRG-RAM writes.
		return !( ram_protect1 == 2 && ram_protect2 == 1 );
	}
};
