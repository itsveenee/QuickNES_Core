
// Nes_Emu 0.7.0. http://www.slack.net/~ant/

#include "Nes_Mapper.h"

#include <cstdio>
#include <string.h>
#include "Nes_Core.h"

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

/*
  New mapping distribution by Sergio Martin (eien86)
  https://github.com/SergioMartin86/jaffarPlus
*/
#include "mappers/mapper000.hpp"
#include "mappers/mapper001.hpp"
#include "mappers/mapper002.hpp"
#include "mappers/mapper003.hpp"
#include "mappers/mapper004.hpp"
#include "mappers/mapper005.hpp"
#include "mappers/mapper007.hpp"
#include "mappers/mapper009.hpp"
#include "mappers/mapper010.hpp"
#include "mappers/mapper011.hpp"
#include "mappers/mapper013.hpp"
#include "mappers/mapper015.hpp"
#include "mappers/mapper016.hpp"
#include "mappers/mapper018.hpp"
#include "mappers/mapper019.hpp"
#include "mappers/mapper021.hpp"
#include "mappers/mapper022.hpp"
#include "mappers/mapper023.hpp"
#include "mappers/mapper024.hpp"
#include "mappers/mapper025.hpp"
#include "mappers/mapper026.hpp"
#include "mappers/mapper027.hpp"
#include "mappers/mapper030.hpp"
#include "mappers/mapper032.hpp"
#include "mappers/mapper033.hpp"
#include "mappers/mapper034.hpp"
#include "mappers/mapper048.hpp"
#include "mappers/mapper060.hpp"
#include "mappers/mapper064.hpp"
#include "mappers/mapper065.hpp"
#include "mappers/mapper066.hpp"
#include "mappers/mapper067.hpp"
#include "mappers/mapper068.hpp"
#include "mappers/mapper069.hpp"
#include "mappers/mapper070.hpp"
#include "mappers/mapper071.hpp"
#include "mappers/mapper072.hpp"
#include "mappers/mapper077.hpp"
#include "mappers/mapper073.hpp"
#include "mappers/mapper075.hpp"
#include "mappers/mapper076.hpp"
#include "mappers/mapper078.hpp"
#include "mappers/mapper079.hpp"
#include "mappers/mapper080.hpp"
#include "mappers/mapper082.hpp"
#include "mappers/mapper085.hpp"
#include "mappers/mapper086.hpp"
#include "mappers/mapper087.hpp"
#include "mappers/mapper088.hpp"
#include "mappers/mapper089.hpp"
#include "mappers/mapper093.hpp"
#include "mappers/mapper094.hpp"
#include "mappers/mapper095.hpp"
#include "mappers/mapper096.hpp"
#include "mappers/mapper097.hpp"
#include "mappers/mapper099.hpp"
#include "mappers/mapper101.hpp"
#include "mappers/mapper103.hpp"
#include "mappers/mapper105.hpp"
#include "mappers/mapper113.hpp"
#include "mappers/mapper118.hpp"
#include "mappers/mapper119.hpp"
#include "mappers/mapper140.hpp"
#include "mappers/mapper152.hpp"
#include "mappers/mapper154.hpp"
#include "mappers/mapper156.hpp"
#include "mappers/mapper157.hpp"
#include "mappers/mapper159.hpp"
/* AURORA_QUICKNES_MAPPERS_162_163_164_V7_20260917 */
#include "mappers/mapper162.hpp"
#include "mappers/mapper163.hpp"
#include "mappers/mapper164.hpp"
#include "mappers/mapper180.hpp"
#include "mappers/mapper184.hpp"
#include "mappers/mapper185.hpp"
#include "mappers/mapper188.hpp"
#include "mappers/mapper190.hpp"
#include "mappers/mapper193.hpp"
#include "mappers/mapper206.hpp"
#include "mappers/mapper207.hpp"
#include "mappers/mapper210.hpp"
#include "mappers/mapper216.hpp"
#include "mappers/mapper232.hpp"
#include "mappers/mapper240.hpp"
#include "mappers/mapper241.hpp"
#include "mappers/mapper244.hpp"
#include "mappers/mapper246.hpp"
#include "mappers/mapper347.hpp"
#include "mappers/aurora_vn46_mapper012.hpp"
#include "mappers/aurora_vn32_mapper029.hpp"
#include "mappers/aurora_vn49_mapper036.hpp"
#include "mappers/aurora_vn34_mapper038.hpp"
#include "mappers/aurora_vn33_mapper041.hpp"
#include "mappers/aurora_vn43_mapper042.hpp"
#include "mappers/aurora_vn41_mapper043.hpp"
#include "mappers/aurora_vn35_mapper047.hpp"
#include "mappers/aurora_vn31_mapper050.hpp"
#include "mappers/aurora_vn40_mapper074.hpp"
#include "mappers/aurora_vn32_mapper107.hpp"
#include "mappers/aurora_vn32_mapper108.hpp"
#include "mappers/aurora_vn32_mapper112.hpp"
#include "mappers/aurora_vn50_mapper132.hpp"
#include "mappers/aurora_vn32_mapper133.hpp"
#include "mappers/aurora_vn37_mapper137.hpp"
#include "mappers/aurora_vn33_mapper138.hpp"
#include "mappers/aurora_vn33_mapper139.hpp"
#include "mappers/aurora_vn33_mapper141.hpp"
#include "mappers/aurora_vn31_mapper142.hpp"
#include "mappers/aurora_vn50_mapper143.hpp"
#include "mappers/aurora_vn33_mapper144.hpp"
#include "mappers/aurora_vn32_mapper145.hpp"
#include "mappers/aurora_vn34_mapper146.hpp"
#include "mappers/aurora_vn49_mapper147.hpp"
#include "mappers/aurora_vn33_mapper148.hpp"
#include "mappers/aurora_vn33_mapper149.hpp"
#include "mappers/aurora_vn32_mapper166.hpp"
#include "mappers/aurora_vn32_mapper167.hpp"
#include "mappers/aurora_vn49_mapper173.hpp"
#include "mappers/aurora_vn53_mapper176.hpp"
#include "mappers/aurora_vn35_mapper177.hpp"
#include "mappers/aurora_vn31_mapper178.hpp"
#include "mappers/aurora_vn44_mapper183.hpp"
#include "mappers/aurora_vn39_mapper192.hpp"
#include "mappers/aurora_vn39_mapper194.hpp"
#include "mappers/aurora_vn52_mapper195.hpp"
#include "mappers/aurora_vn39_mapper196.hpp"
#include "mappers/aurora_vn36_mapper208.hpp"
#include "mappers/aurora_vn48_mapper223.hpp"
#include "mappers/aurora_vn48_mapper224.hpp"
#include "mappers/aurora_vn42_mapper227.hpp"
#include "mappers/aurora_vn45_mapper228.hpp"
#include "mappers/aurora_vn35_mapper238.hpp"
#include "mappers/aurora_vn42_mapper242.hpp"
#include "mappers/aurora_vn40_mapper243.hpp"
#include "mappers/aurora_vn34_mapper245.hpp"
#include "mappers/aurora_vn51_mapper258.hpp"
#include "mappers/aurora_vn35_mapper263.hpp"
#include "mappers/aurora_vn55_mapper268.hpp"
#include "mappers/aurora_vn28_mapper302.hpp"
#include "mappers/aurora_vn29_mapper303.hpp"
#include "mappers/aurora_vn31_mapper304.hpp"
#include "mappers/aurora_vn28_mapper305.hpp"
#include "mappers/aurora_vn28_mapper306.hpp"
#include "mappers/aurora_vn29_mapper309.hpp"
#include "mappers/aurora_vn29_mapper312.hpp"
#include "mappers/aurora_vn29_mapper346.hpp"
#include "mappers/aurora_vn30_mapper415.hpp"
#include "mappers/aurora_vn31_mapper525.hpp"
#include "mappers/aurora_vn29_mapper538.hpp"
#include "mappers/aurora_vn28_mapper549.hpp"
#include "mappers/aurora_vn37_mapper551.hpp"
#include "mappers/aurora_vn45_mapper553.hpp"
#include "mappers/aurora_vn54_mapper554.hpp"
#include "mappers/aurora_vn37_mapper557.hpp"
#include "mappers/aurora_vN118_mapper35_209_FAST_CANDIDATE.hpp"
#include "mappers/aurora_vn71_mapper272_CANDIDATE.hpp"
#include "mappers/aurora_vn66_mapper540_quicknes_CANDIDATE.hpp"
#include "mappers/aurora_vn71_mapper558_CANDIDATE.hpp"
#include "mappers/aurora_vn106_mapper219.hpp"
#include "mappers/aurora_vn107_mapper249.hpp"
#include "mappers/aurora_37_40_91_150.hpp"
#include "mappers/aurora_mapper090_160.hpp"
#include "mappers/aurora_neighbor_gaps.hpp"
#include "mappers/aurora_green_expansion.hpp"

Nes_Mapper::Nes_Mapper()
{
	emu_ = NULL;
	static char c;
	state = &c; // TODO: state must not be null?
	state_size = 0;
}

Nes_Mapper::~Nes_Mapper()
{
}

// Sets mirroring, maps first 8K CHR in, first and last 16K of PRG,
// intercepts writes to upper half of memory, and clears registered state.
void Nes_Mapper::default_reset_state()
{
	int mirroring = cart_->mirroring();
	if ( mirroring & 8 )
		mirror_full();
	else if ( mirroring & 1 )
		mirror_vert();
	else
		mirror_horiz();
	
	set_chr_bank( 0, bank_8k, 0 );
	
	set_prg_bank( 0x8000, bank_16k, 0 );
	set_prg_bank( 0xC000, bank_16k, last_bank );
	
	intercept_writes( 0x8000, 0x8000 );
	
	memset( state, 0, state_size );
}

void Nes_Mapper::reset()
{
	default_reset_state();
	reset_state();
	apply_mapping();
}

void mapper_state_t::write( const void* p, unsigned long s )
{
	size = s;
	memcpy( data, p, s );
}

int mapper_state_t::read( void* p, unsigned long s ) const
{
	if ( (long) s > size )
		s = size;
	memcpy( p, data, s );
	return s;
}

void Nes_Mapper::save_state( mapper_state_t& out )
{
	out.write( state, state_size );
}

void Nes_Mapper::load_state( mapper_state_t const& in )
{
	default_reset_state();
	read_state( in );
	apply_mapping();
}

void Nes_Mapper::read_state( mapper_state_t const& in )
{
	memset( state, 0, state_size );
	in.read( state, state_size );
	apply_mapping();
}

// Timing

void Nes_Mapper::irq_changed() { emu_->irq_changed(); }
	
nes_time_t Nes_Mapper::next_irq( nes_time_t ) { return no_irq; }

void Nes_Mapper::a12_clocked() { }

/* AURORA_V3_2_MMC3_PPU_NOTIFY_ISOLATION_20260831 */
bool Nes_Mapper::ppu_ctrl_mask_irq_sensitive() const { return false; }

/* AURORA_FINAL10_DEFAULT_HOOKS_V1 */
long Nes_Mapper::chr_ram_size() const { return 0x2000; }
long Nes_Mapper::aux_chr_ram_size() const { return 0; }
bool Nes_Mapper::needs_vram_address_hook() const { return false; }
void Nes_Mapper::vram_address_changed( nes_addr_t ) { }

void Nes_Mapper::run_until( nes_time_t ) { }

void Nes_Mapper::end_frame( nes_time_t ) { }

bool Nes_Mapper::ppu_enabled() const { return emu().ppu.w2001 & 0x08; }

// Sound

int Nes_Mapper::channel_count() const { return 0; }

void Nes_Mapper::set_channel_buf( int, Blip_Buffer* ) { }

void Nes_Mapper::set_treble( blip_eq_t const& ) { }

// Memory mapping

void Nes_Mapper::set_prg_bank( nes_addr_t addr, bank_size_t bs, int bank )
{
	int bank_size = 1 << bs;
	int bank_count = cart_->prg_size() >> bs;

	/* AURORA_SMALL_PRG_MIRROR_V1
	 * An undersized physical PRG ROM can legitimately be mirrored into a
	 * larger mapper window (Galaxian: 8 KiB chip in a 16 KiB NROM bank).
	 * The loader materializes that mirror for mapper 0, but this guard also
	 * prevents bank %= 0 on any unusual small image.
	 */
	if ( bank_count <= 0 )
	{
		long physical_size = cart_->prg_size();

		if ( physical_size <= 0 ||
		     (physical_size % Nes_Cpu::page_size) != 0 )
			return;

		/* Mirror the physical ROM repeatedly into the larger CPU window.
		 * map_code() works in 2 KiB pages, so no read can run past the
		 * cartridge allocation. */
		int mapped = 0;
		while ( mapped < bank_size )
		{
			int chunk = (int) physical_size;
			if ( chunk > bank_size - mapped )
				chunk = bank_size - mapped;

			emu().map_code( addr + mapped, chunk, cart_->prg() );
			mapped += chunk;
		}

		if ( unsigned (addr - 0x6000) < 0x2000 )
			emu().enable_prg_6000();
		return;
	}

	if ( bank < 0 )
	{
		bank %= bank_count;
		if ( bank < 0 )
			bank += bank_count;
	}
	else if ( bank >= bank_count )
	{
		bank %= bank_count;
	}

	emu().map_code( addr, bank_size, cart_->prg() + (bank << bs) );

	if ( unsigned (addr - 0x6000) < 0x2000 )
		emu().enable_prg_6000();
}

void Nes_Mapper::set_chr_bank( nes_addr_t addr, bank_size_t bs, int bank )
{
	emu().ppu.render_until( emu().clock() ); 
	emu().ppu.set_chr_bank( addr, 1 << bs, bank << bs );
}

void Nes_Mapper::set_chr_bank_ex( nes_addr_t addr, bank_size_t bs, int bank )
{
	emu().ppu.render_until( emu().clock() ); 
	emu().ppu.set_chr_bank_ex( addr, 1 << bs, bank << bs );
}

void Nes_Mapper::set_chr_ram_bank( nes_addr_t addr, bank_size_t bs, int bank )
{
	emu().ppu.render_until( emu().clock() );
	emu().ppu.set_chr_ram_bank( addr, 1 << bs, bank );
}

void Nes_Mapper::set_chr_read_or( int mask )
{
	emu().ppu.render_until( emu().clock() );
	emu().ppu.set_chr_read_or( mask );
}


void Nes_Mapper::set_exgrafix( uint8_t const* exram, int chr_high )
{
	emu().ppu.render_until( emu().clock() );
	emu().ppu.set_exgrafix( exram, chr_high );
}

void Nes_Mapper::mirror_manual( int page0, int page1, int page2, int page3 )
{
	emu().ppu.render_bg_until( emu().clock() ); 
	emu().ppu.set_nt_banks( page0, page1, page2, page3 );
}

void Nes_Mapper::mirror_chr( int page, int chr_1k_bank )
{
	emu().ppu.render_bg_until( emu().clock() );
	emu().ppu.set_nt_bank_chr( page, chr_1k_bank );
}


#ifndef NDEBUG
int Nes_Mapper::handle_bus_conflict( nes_addr_t addr, int data )
{
	return data;
}
#endif


Nes_Mapper* Nes_Mapper::create( Nes_Cart const* cart, Nes_Core* emu )
{
  // Getting cartdrige mapper code
  int mapperCode = cart->mapper_code();
	
  // Storage for the mapper, NULL by default	
  Nes_Mapper* mapper = NULL;

  // Now checking if the detected mapper code is supported. A switch
  // is faster than the old if-chain (which paid for every comparison
  // unconditionally on every cart load) and guards against the silent
  // footgun where two entries for the same mapperCode would have both
  // executed -- the second would have leaked the first.
  /* AURORA_VN36_BASE_FAMICOM_ONLY:
 * No firmware/copy hardware or nonstandard peripherals. */
if ((mapperCode == 12 && cart->submapper_code() != 0) ||
    (mapperCode == 178 && (cart->submapper_code() == 1 || cart->submapper_code() == 2 || cart->submapper_code() == 3)) ||
    (mapperCode == 208 && cart->submapper_code() > 1)) return NULL;
/* AURORA_VN36_END_FAMICOM_ONLY */
/* AURORA_VN43_GREEN_BERET_ONLY_BEGIN */
if (mapperCode == 42 && (
    (cart->submapper_code() != 0 && cart->submapper_code() != 2) ||
    cart->chr_size() != 0 ||
    (cart->prg_size() != 160 * 1024 && cart->prg_size() != 256 * 1024)))
    return NULL;
/* AURORA_VN43_GREEN_BERET_ONLY_END */
/* AURORA_VN53_176_FK_ONLY_BEGIN */
/* Only FK23C/FK-xxx variants 0 and 1. Legacy iNES dumps with
   very large CHR-RAM banks cannot be mistaken for subtype zero. */
if (mapperCode == 176 &&
    (cart->submapper_code() > 1 ||
     (cart->submapper_code() == 0 && cart->chr_size() == 0 &&
      cart->prg_size() >= 0x400000L)))
    return NULL;
/* AURORA_VN53_176_FK_ONLY_END */
/* AURORA_VN55_M268_SUBMAPPER_GUARD_BEGIN */
  /* COOLBOY/MINDKIDS original PCB submappers 0/1 only. */
  if (mapperCode == 268 && cart->submapper_code() > 1) return NULL;
  /* AURORA_VN55_M268_SUBMAPPER_GUARD_END */
  switch ( mapperCode )
  {
    case   0: mapper = new Mapper000(); break;
    case   1: mapper = new Mapper001(); break;
    case   2: mapper = new Mapper002(); break;
    case   3: mapper = new Mapper003(); break;
    case  35: mapper = new Mapper035(); break;
    case   4: mapper = (cart->submapper_code() == 5) ? static_cast<Nes_Mapper*>(new AuroraVn107Mapper249(true)) : static_cast<Nes_Mapper*>(new Mapper004()); break;
    case   5: mapper = new Mapper005(); break;
    case   7: mapper = new Mapper007(); break;
    case   9: mapper = new Mapper009(); break;
    case  10: mapper = new Mapper010(); break;
    case  11: mapper = new Mapper011(); break;
    case  13: mapper = new Mapper013(); break;
    case  15: mapper = new Mapper015(); break;
    case  16: mapper = new Mapper016(); break;
    case  18: mapper = new Mapper018(); break;
    case  19: mapper = new Mapper019(); break;
    case  21: mapper = new Mapper021(); break;
    case  22: mapper = new Mapper022(); break;
    case  23: mapper = new Mapper023(); break;
    case  24: mapper = new Mapper024(); break;
    case  25: mapper = new Mapper025(); break;
    case  26: mapper = new Mapper026(); break;
    case  27: mapper = new Mapper027(); break;
    case  30: mapper = new Mapper030(); break;
    case  32: mapper = new Mapper032(); break;
    case  33: mapper = new Mapper033(); break;
    case  34: mapper = new Mapper034(); break;
    case  48: mapper = new Mapper048(); break;
    case  60: mapper = new Mapper060(); break;
    case  64: mapper = new Mapper064(); break;
    case  65: mapper = new Mapper065(); break;
    case  66: mapper = new Mapper066(); break;
    case  67: mapper = new Mapper067(); break;
    case  68: mapper = new Mapper068(); break;
    case  69: mapper = new Mapper069(); break;
    case  70: mapper = new Mapper070(); break;
    case  71: mapper = new Mapper071(); break;
    case  72: mapper = new Mapper072(); break;
    case  77: mapper = new Mapper077(); break;
    case  73: mapper = new Mapper073(); break;
    case  75: mapper = new Mapper075(); break;
    case  76: mapper = new Mapper076(); break;
    case  78: mapper = new Mapper078(); break;
    case  79: mapper = new Mapper079(); break;
    case  80: mapper = new Mapper080(); break;
    case  82: mapper = new Mapper082(); break;
    case  85: mapper = new Mapper085(); break;
    case  86: mapper = new Mapper086(); break;
    case  87: mapper = new Mapper087(); break;
    case  88: mapper = new Mapper088(); break;
    case  89: mapper = new Mapper089(); break;
    case  92: mapper = new Mapper092(); break;
    case  93: mapper = new Mapper093(); break;
    case  94: mapper = new Mapper094(); break;
    case  95: mapper = new Mapper095(); break;
    case  96: mapper = new Mapper096(); break;
    case  97: mapper = new Mapper097(); break;
    case  99: mapper = new Mapper099(); break;
    case 101: mapper = new Mapper101(); break;
    case 103: mapper = new Mapper103(); break;
    case 105: mapper = new Mapper105(); break;
    case 113: mapper = new Mapper113(); break;
    case 118: mapper = new Mapper118(); break;
    case 119: mapper = new Mapper119(); break;
    case 140: mapper = new Mapper140(); break;
    case 151: mapper = new Mapper075(); break;
    case 152: mapper = new Mapper152(); break;
    case 153: mapper = new Mapper153(); break;
    case 154: mapper = new Mapper154(); break;
    case 155: mapper = new Mapper001(); break;
    case 156: mapper = new Mapper156(); break;
    case 157: mapper = new Mapper157(); break;
    case 158: mapper = new Mapper158(); break;
    case 159: mapper = new Mapper159(); break;
    case 162: mapper = new Mapper162(); break;
    case 163: mapper = new Mapper163(); break;
    case 164: mapper = new Mapper164(); break;
    case 180: mapper = new Mapper180(); break;
    case 184: mapper = new Mapper184(); break;
    case 185: mapper = new Mapper185(); break;
    case 188: mapper = new Mapper188(); break;
    case 190: mapper = new Mapper190(); break;
    case 193: mapper = new Mapper193(); break;
    case 206: mapper = new Mapper206(); break;
    case 207: mapper = new Mapper207(); break;
    case 210: mapper = new Mapper210(); break;
    case 216: mapper = new Mapper216(); break;
    case  83: mapper = new Mapper083(); break;
    case 114: mapper = new Mapper114(); break;
    case 115: mapper = new Mapper115(); break;
    case 116: mapper = new Mapper116(); break;
    case 117: mapper = new Mapper117(); break;
    case 121: mapper = new Mapper121(); break;
    case 123: mapper = new Mapper123(); break;
    case 189: mapper = new Mapper189(); break;
    case 197: mapper = new Mapper197(); break;
    case 215: mapper = new Mapper215(); break;
    case 209: mapper = new Mapper209(); break;
    case 219: mapper = new AuroraVn106Mapper219(); break;
    case 222: mapper = new Mapper222(); break;
    case  14: mapper = new Mapper014(); break;
    case  17: mapper = new Mapper017(); break;
    case  81: mapper = new Mapper081(); break;
    case 104: mapper = new Mapper104(); break;
    case 120: mapper = new Mapper120(); break;
    case 122: mapper = new Mapper122(); break;
    case 161: mapper = new Mapper161(); break;
    case 187: mapper = new Mapper187(); break;
    case 90: mapper = new Mapper090(); break;
    case 160: mapper = new Mapper160(); break;
    case 37: mapper = new Mapper037(); break;
    case 40: mapper = new Mapper040(); break;
    case 91: mapper = new Mapper091(); break;
    case 150: mapper = new Mapper150(); break;
    case 232: mapper = new Mapper232(); break;
    case 240: mapper = new Mapper240(); break;
    case 241: mapper = new Mapper241(); break;
    case 244: mapper = new Mapper244(); break;
    case 246: mapper = new Mapper246(); break;
    case 347: mapper = new Mapper347(); break;
    case 12: mapper = new AuroraVn46Mapper012(); break;
    case 29: mapper = new AuroraVn32Mapper029(); break;
    case 36: mapper = new AuroraVn49Mapper036(); break;
    case 38: mapper = new AuroraVn34Mapper038(); break;
    case 41: mapper = new AuroraVn33Mapper041(); break;
    case 42: mapper = new AuroraVn43Mapper042(); break;
    case 43: mapper = new AuroraVn41Mapper043(); break;
    case 47: mapper = new AuroraVn35Mapper047(); break;
    case 50: mapper = new AuroraVn31Mapper050(); break;
    case 74: mapper = new AuroraVn40Mapper074(); break;
    case 107: mapper = new AuroraVn32Mapper107(); break;
    case 108: mapper = new AuroraVn32Mapper108(); break;
    case 112: mapper = new AuroraVn32Mapper112(); break;
    case 132: mapper = new AuroraVn50Mapper132(); break;
    case 133: mapper = new AuroraVn32Mapper133(); break;
    case 137: mapper = new AuroraVn37Mapper137(); break;
    case 138: mapper = new AuroraVn33Mapper138(); break;
    case 139: mapper = new AuroraVn33Mapper139(); break;
    case 141: mapper = new AuroraVn33Mapper141(); break;
    case 142: mapper = new AuroraVn31Mapper142(); break;
    case 143: mapper = new AuroraVn50Mapper143(); break;
    case 144: mapper = new AuroraVn33Mapper144(); break;
    case 145: mapper = new AuroraVn32Mapper145(); break;
    case 146: mapper = new AuroraVn34Mapper146(); break;
    case 147: mapper = new AuroraVn49Mapper147(); break;
    case 148: mapper = new AuroraVn33Mapper148(); break;
    case 149: mapper = new AuroraVn33Mapper149(); break;
    case 166: mapper = new AuroraVn32Mapper166(); break;
    case 167: mapper = new AuroraVn32Mapper167(); break;
    case 173: mapper = new AuroraVn49Mapper173(); break;
    case 176: mapper = new AuroraVn53Mapper176(); break;
    case 177: mapper = new AuroraVn35Mapper177(); break;
    case 178: mapper = new AuroraVn31Mapper178(); break;
    case 183: mapper = new AuroraVn44Mapper183(); break;
    case 192: mapper = new AuroraVn39Mapper192(); break;
    case 194: mapper = new AuroraVn39Mapper194(); break;
    case 195: mapper = new AuroraVn52Mapper195(); break;
    case 196: mapper = new AuroraVn39Mapper196(); break;
    case 208: mapper = new AuroraVn36Mapper208(); break;
    case 223: mapper = new AuroraVn48Mapper223(); break;
    case 224: mapper = new AuroraVn48Mapper224(); break;
    case 227: mapper = new AuroraVn42Mapper227(); break;
    case 228: mapper = new AuroraVn45Mapper228(); break;
    case 238: mapper = new AuroraVn35Mapper238(); break;
    case 242: mapper = new AuroraVn42Mapper242(); break;
    case 243: mapper = new AuroraVn40Mapper243(); break;
    case 245: mapper = new AuroraVn34Mapper245(); break;
    case 249: mapper = new AuroraVn107Mapper249(); break;
    case 258: mapper = new AuroraVn51Mapper258(); break;
    case 263: mapper = new AuroraVn35Mapper263(); break;
    case 268: mapper = new AuroraVn55Mapper268(); break;
    case 272: mapper = new Mapper272(); break;
    case 302: mapper = new AuroraVn28Mapper302(); break;
    case 303: mapper = new AuroraVn29Mapper303(); break;
    case 304: mapper = new AuroraVn31Mapper304(); break;
    case 305: mapper = new AuroraVn28Mapper305(); break;
    case 306: mapper = new AuroraVn28Mapper306(); break;
    case 309: mapper = new AuroraVn29Mapper309(); break;
    case 312: mapper = new AuroraVn29Mapper312(); break;
    case 346: mapper = new AuroraVn29Mapper346(); break;
    case 415: mapper = new AuroraVn30Mapper415(); break;
    case 525: mapper = new AuroraVn31Mapper525(); break;
    case 538: mapper = new AuroraVn29Mapper538(); break;
    case 540: mapper = new AuroraVn66Mapper540Candidate(); break;
    case 549: mapper = new AuroraVn28Mapper549(); break;
    case 551: mapper = new AuroraVn37Mapper551(); break;
    case 553: mapper = new AuroraVn45Mapper553(); break;
    case 554: mapper = new AuroraVn54Mapper554(); break;
    case 557: mapper = new AuroraVn37Mapper557(); break;
    case 558: mapper = new Mapper558(); break;
    case 552: mapper = new Mapper552(); break;
    default: break;
  }

  // If no mapper was found, return null (error) now 
  if (mapper == NULL)
  {
	fprintf(stderr, "Could not find mapper for code: %u\n", mapperCode);
    return NULL;
  } 

  // Assigning backwards pointers to cartdrige and emulator now
  mapper->cart_ = cart;
  mapper->emu_ = emu;

  // Returning successfully created mapper
  return mapper;
}
