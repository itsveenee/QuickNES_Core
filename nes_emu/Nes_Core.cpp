
// Nes_Emu 0.7.0. http://www.slack.net/~ant/

#include "Nes_Core.h"

#include <string.h>
#include "Nes_Mapper.h"
#include "Nes_State.h"

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

extern const char unsupported_mapper [] = "Unsupported mapper";

bool const wait_states_enabled = true;
bool const single_instruction_mode = false; // for debugging irq/nmi timing issues

const int unmapped_fill = Nes_Cpu::page_wrap_opcode;

unsigned const low_ram_size = 0x800;
unsigned const sram_end     = 0x8000;

Nes_Core::Nes_Core() : ppu( this )
{
	cart = NULL;
	impl = NULL;
	mapper = NULL;
	memset( &nes, 0, sizeof nes );
	memset( &joypad, 0, sizeof joypad );
}

const char * Nes_Core::init()
{
	if ( !impl )
	{
		CHECK_ALLOC( impl = new impl_t );
		impl->apu.dmc_reader( read_dmc, this );
		impl->apu.irq_notifier( apu_irq_changed, this );
	}
	
	return 0;
}

void Nes_Core::close()
{
	cart = NULL;
	delete mapper;
	mapper = NULL;
	
	ppu.close_chr();
	
	disable_rendering();
}

const char * Nes_Core::open( Nes_Cart const* new_cart )
{
	close();
	
	RETURN_ERR( init() );
	
	mapper = Nes_Mapper::create( new_cart, this );
	if ( !mapper ) 
		return unsupported_mapper;

	ppu.set_vram_address_hook( mapper->needs_vram_address_hook() ? mapper : NULL );

	RETURN_ERR( ppu.open_chr( new_cart->chr(), new_cart->chr_size() ) );
	/* AURORA_FINAL10_SPECIAL_CHR_CONFIG_V1
	 * Legacy open_chr above remains the normal loader for every mapper. */
	if ( (new_cart->chr_size() == 0 && mapper->chr_ram_size() != 0x2000) ||
	     mapper->aux_chr_ram_size() > 0 )
	{
		RETURN_ERR( ppu.configure_special_chr(
			new_cart->chr(), new_cart->chr_size(),
			mapper->chr_ram_size(), mapper->aux_chr_ram_size() ) );
	}
	
	cart = new_cart;
	memset( impl->unmapped_page, unmapped_fill, sizeof impl->unmapped_page );
	reset( true, true );

	return 0;
}

Nes_Core::~Nes_Core()
{
	close();
	delete impl;
}

void Nes_Core::save_state( Nes_State_* out ) const
{
	out->clear();
	
	out->nes = nes;
	out->nes_valid = true;
	
	*out->cpu = cpu::r;
	out->cpu_valid = true;
	
	*out->joypad = joypad;
	out->joypad_valid = true;
	
	impl->apu.save_state( out->apu );
	out->apu_valid = true;
	
	ppu.save_state( out );
	
	memcpy( out->ram, cpu::low_mem, out->ram_size );
	out->ram_valid = true;
	
	out->sram_size = 0;
	if ( sram_present )
	{
		long bytes = impl_t::sram_window_size;
		if ( cart && cart->battery_ram_size() > bytes )
			bytes = cart->battery_ram_size();
		if ( bytes > impl_t::sram_size )
			bytes = impl_t::sram_size;
		out->sram_size = bytes;
		memcpy( out->sram, impl->sram, out->sram_size );
	}
	
	out->mapper->size = 0;
	mapper->save_state( *out->mapper );
	out->mapper_valid = true;
}

void Nes_Core::save_state( Nes_State* out ) const
{
	save_state( reinterpret_cast<Nes_State_*>(out) );
}

void Nes_Core::load_state( Nes_State_ const& in )
{
	disable_rendering();
	error_count = 0;
	
	if ( in.nes_valid )
		nes = in.nes;
	
	// always use frame count
	ppu.burst_phase = 0; // avoids shimmer when seeking to same time over and over
	nes.frame_count = in.nes.frame_count;
	if ( (frame_count_t) nes.frame_count == invalid_frame_count )
		nes.frame_count = 0;
	
	if ( in.cpu_valid )
		cpu::r = *in.cpu;
	
	if ( in.joypad_valid )
		joypad = *in.joypad;
	
	if ( in.apu_valid )
	{
		impl->apu.load_state( *in.apu );
		// prevent apu from running extra at beginning of frame
		impl->apu.end_frame( -(int) nes.timestamp / ppu_overclock );
	}
	else
	{
		impl->apu.reset();
	}
	
	ppu.load_state( in );
	
	if ( in.ram_valid )
		memcpy( cpu::low_mem, in.ram, in.ram_size );
	
	sram_present = false;
	if ( in.sram_size )
	{
		sram_present = true;
		memcpy( impl->sram, in.sram, min( (int) in.sram_size, (int) sizeof impl->sram ) );
		sram_bank = 0;
		enable_sram( true ); // mapper can override (read-only, unmapped, etc.)
	}
	
	if ( in.mapper_valid ) // restore last since it might reconfigure things
		mapper->load_state( *in.mapper );
}

void Nes_Core::enable_prg_6000()
{
	sram_writable = 0;
	sram_readable = 0;
	lrom_readable = 0x8000;
}

void Nes_Core::enable_sram( bool b, bool read_only )
{
	sram_writable = 0;
	if ( b )
	{
		if ( !sram_present )
		{
			sram_present = true;
			memset( impl->sram, 0xFF, impl->sram_size );
		}
		sram_readable = sram_end;
		if ( !read_only )
			sram_writable = sram_end;
		cpu::map_code(
			0x6000, impl_t::sram_window_size,
			impl->sram + sram_bank * impl_t::sram_window_size );
	}
	else
	{
		sram_readable = 0;
		for ( int i = 0; i < impl_t::sram_window_size; i += cpu::page_size )
			cpu::map_code( 0x6000 + i, cpu::page_size, impl->unmapped_page );
	}
}

void Nes_Core::set_sram_bank( int bank )
{
	const int bank_count = impl_t::sram_size / impl_t::sram_window_size;
	if ( bank < 0 ) bank = 0;
	sram_bank = (unsigned) bank % (unsigned) bank_count;

	/* Opcode fetches use the CPU's mapped code pages, while ordinary
	 * data reads/writes use sram_bank in nes_cpu_io.h. Keep both views
	 * on the same SXROM bank. */
	if ( sram_readable )
		cpu::map_code(
			0x6000, impl_t::sram_window_size,
			impl->sram + sram_bank * impl_t::sram_window_size );
}

// Unmapped memory

#ifndef NDEBUG
static nes_addr_t last_unmapped_addr;
#endif

inline void Nes_Core::cpu_adjust_time( int n )
{
	ppu_2002_time   -= n;
	cpu_time_offset += n;
	cpu::reduce_limit( n );
}

// I/O and sound

int Nes_Core::read_dmc( void* data, nes_addr_t addr )
{
	Nes_Core* emu = (Nes_Core*) data;
	int result = *emu->cpu::get_code( addr );
	if ( wait_states_enabled )
		emu->cpu_adjust_time( 4 );
	return result;
}

void Nes_Core::apu_irq_changed( void* emu )
{
	((Nes_Core*) emu)->irq_changed();
}

/* SNESTICLE_DUTY_SWAP_BEGIN
 * Optional Famiclone pulse-duty swap supplied by the SNESticle frontend.
 * It deliberately changes only future $4000/$4004 writes; unlike the old
 * InfoNES hook, QuickNES does not expose its private live APU register image
 * for an immediate replay when the option is toggled.
 */
static bool s_snesticle_duty_swap = false;

extern "C" void quicknes_snesticle_set_duty_swap(int enable)
{
    s_snesticle_duty_swap = enable ? true : false;
}

/* AURORA_FAMICOM_MIC_CFG41_20260828
 * The original Famicom controller II microphone is an immediate one-bit
 * signal on $4016 D2. A held host button alternates the level on successive
 * $4016 reads, matching the conventional emulator approximation used by
 * games that detect microphone transitions rather than a fixed polarity. */
static bool s_snesticle_microphone = false;
static bool s_snesticle_microphone_phase = false;

extern "C" void quicknes_snesticle_set_microphone(int enable)
{
    const bool next = enable ? true : false;
    if (next != s_snesticle_microphone)
        s_snesticle_microphone_phase = false;
    s_snesticle_microphone = next;
}

static int quicknes_snesticle_microphone_bit(int addr)
{
    if (addr != 0x4016 || !s_snesticle_microphone)
        return 0;
    s_snesticle_microphone_phase = !s_snesticle_microphone_phase;
    return s_snesticle_microphone_phase ? 0x04 : 0;
}
/* SNESTICLE_DUTY_SWAP_END */

/* AURORA_QN_EXT_TURBOFILE_VAUS_V2_20260828
 * Famicom expansion-port devices for Aurora's native QuickNES path.
 *
 * Default EXT device: ASCII Turbo File, 8 KiB battery-backed SRAM.
 * Arkanoid (Japan) is the sole automatic exception and replaces it with
 * the Famicom Vaus controller. Normal pad bits remain on D0; expansion
 * signals are ORed onto the authentic extra data bits.
 *
 * Turbo File protocol follows the documented/Mesen model:
 *   $4016 D1 low -> position reset
 *   $4016 D2 falling edge -> write D0, then advance one bit
 *   $4017 D2 -> current SRAM bit
 * Merely polling controllers never raises D2, so it never marks the file
 * dirty and therefore does not create a save file.
 */
enum
{
    QN_TURBOFILE_BYTES = 0x2000,
    QN_TURBOFILE_BITS  = QN_TURBOFILE_BYTES * 8
};

static unsigned char s_snesticle_turbofile[QN_TURBOFILE_BYTES];
static bool s_snesticle_turbofile_initialized = false;
static bool s_snesticle_turbofile_dirty = false;
static bool s_snesticle_turbofile_protocol_armed = false;
static unsigned int s_snesticle_turbofile_position = 0;
static unsigned char s_snesticle_turbofile_last_write = 0;

static bool s_snesticle_arkanoid_enabled = false;
static bool s_snesticle_arkanoid_strobe = false;
static bool s_snesticle_arkanoid_fire = false;
static unsigned int s_snesticle_arkanoid_value = 0xA4;
static unsigned int s_snesticle_arkanoid_shift = 0xA4;

static void quicknes_snesticle_turbofile_init(void)
{
    if (!s_snesticle_turbofile_initialized)
    {
        memset(s_snesticle_turbofile, 0xFF,
               sizeof(s_snesticle_turbofile));
        s_snesticle_turbofile_initialized = true;
    }
}

extern "C" unsigned char *quicknes_snesticle_ext_turbofile_data(void)
{
    quicknes_snesticle_turbofile_init();
    return s_snesticle_turbofile;
}

extern "C" int quicknes_snesticle_ext_turbofile_dirty(void)
{
    return s_snesticle_turbofile_dirty ? 1 : 0;
}

extern "C" void quicknes_snesticle_ext_turbofile_clear_dirty(void)
{
    s_snesticle_turbofile_dirty = false;
}

extern "C" void quicknes_snesticle_ext_set_arkanoid(int enable)
{
    s_snesticle_arkanoid_enabled = enable ? true : false;
    s_snesticle_arkanoid_strobe = false;
    s_snesticle_arkanoid_shift = s_snesticle_arkanoid_value;
    s_snesticle_turbofile_position = 0;
    s_snesticle_turbofile_last_write = 0;
    s_snesticle_turbofile_protocol_armed = false;
}

extern "C" void quicknes_snesticle_ext_set_arkanoid_state(
    unsigned int paddle, int fire)
{
    if (paddle < 0x54U) paddle = 0x54U;
    if (paddle > 0xF4U) paddle = 0xF4U;
    s_snesticle_arkanoid_value = paddle;
    s_snesticle_arkanoid_fire = fire ? true : false;
}

extern "C" void quicknes_snesticle_ext_reset_bus(void)
{
    s_snesticle_arkanoid_strobe = false;
    s_snesticle_arkanoid_shift = s_snesticle_arkanoid_value;
    s_snesticle_turbofile_position = 0;
    s_snesticle_turbofile_last_write = 0;
    s_snesticle_turbofile_protocol_armed = false;
}

static void quicknes_snesticle_ext_write(int data)
{
    if (s_snesticle_arkanoid_enabled)
    {
        const bool next_strobe = (data & 0x01) != 0;
        if (s_snesticle_arkanoid_strobe && !next_strobe)
            s_snesticle_arkanoid_shift = s_snesticle_arkanoid_value;
        s_snesticle_arkanoid_strobe = next_strobe;
        return;
    }

    quicknes_snesticle_turbofile_init();

    if (!(data & 0x02))
    {
        s_snesticle_turbofile_position = 0;
        s_snesticle_turbofile_protocol_armed = false;
    }
    else if (!(s_snesticle_turbofile_last_write & 0x02))
    {
        /* The documented reset sequence ends 00 -> 02. Requiring that
         * release before persistence avoids treating ordinary joypad strobes
         * (01 -> 00) as Turbo File use merely because the device is always
         * attached in Aurora. */
        s_snesticle_turbofile_protocol_armed = true;
    }

    if (s_snesticle_turbofile_protocol_armed &&
        (data & 0x02) &&
        !(data & 0x04) &&
        (s_snesticle_turbofile_last_write & 0x04))
    {
        const unsigned int byte_index =
            s_snesticle_turbofile_position >> 3;
        const unsigned int bit_mask =
            1U << (s_snesticle_turbofile_position & 7U);

        if (data & 0x01)
            s_snesticle_turbofile[byte_index] |= (unsigned char)bit_mask;
        else
            s_snesticle_turbofile[byte_index] &=
                (unsigned char)~bit_mask;

        s_snesticle_turbofile_position =
            (s_snesticle_turbofile_position + 1U) &
            (QN_TURBOFILE_BITS - 1U);
        s_snesticle_turbofile_dirty = true;
    }

    s_snesticle_turbofile_last_write = (unsigned char)data;
}

static int quicknes_snesticle_ext_read(int addr)
{
    if (s_snesticle_arkanoid_enabled)
    {
        if (addr == 0x4016)
            return s_snesticle_arkanoid_fire ? 0x02 : 0;

        if (addr == 0x4017)
        {
            const int output =
                (int)(((~s_snesticle_arkanoid_shift) >> 6) & 0x02U);
            s_snesticle_arkanoid_shift <<= 1;
            return output;
        }
        return 0;
    }

    if (addr == 0x4017)
    {
        quicknes_snesticle_turbofile_init();
        return (int)(((s_snesticle_turbofile[
            s_snesticle_turbofile_position >> 3] >>
            (s_snesticle_turbofile_position & 7U)) & 0x01U) << 2);
    }

    return 0;
}


void Nes_Core::write_io( nes_addr_t addr, int data )
{
	// sprite dma
	if ( addr == 0x4014 )
	{
		ppu.dma_sprites( clock(), cpu::get_code( data * 0x100 ) );
		cpu_adjust_time( 513 );
		return;
	}
	
	// joypad strobe
	if ( addr == 0x4016 )
	{
		/* AURORA_QN_EXT_TURBOFILE_VAUS_V2_20260828 */
		quicknes_snesticle_ext_write(data);

		// if strobe goes low, latch data
		if ( joypad.w4016 & 1 & ~data )
		{
			joypad_read_count++;
			joypad.joypad_latches [0] = current_joypad [0];
			joypad.joypad_latches [1] = current_joypad [1];
		}
		joypad.w4016 = data;
		return;
	}
	
	// apu
	if ( unsigned (addr - impl->apu.start_addr) <= impl->apu.end_addr - impl->apu.start_addr )
	{
		/* SNESTICLE_DUTY_SWAP_WRITE */
		if (s_snesticle_duty_swap &&
		    (addr == 0x4000 || addr == 0x4004))
		{
			int duty = (data >> 6) & 3;

			if (duty == 1)
				duty = 2;
			else if (duty == 2)
				duty = 1;

			data = (data & 0x3F) | (duty << 6);
		}

		impl->apu.write_register( clock(), addr, data );
		if ( wait_states_enabled )
		{
			if ( addr == 0x4010 || (addr == 0x4015 && (data & 0x10)) )
			{
				impl->apu.run_until( clock() + 1 );
				event_changed();
			}
		}
		return;
	}
}

int Nes_Core::read_io( nes_addr_t addr )
{
	/* AURORA_NES_STROBE_HIGH_V1
	 * With $4016 strobe held high, the NES controller port exposes the
	 * current A button continuously instead of shifting the saved latch.
	 */
	if ( (addr & 0xFFFE) == 0x4016 )
	{
		const int mic = quicknes_snesticle_microphone_bit(addr);
		const int ext = quicknes_snesticle_ext_read(addr);

		if ( joypad.w4016 & 1 )
			return (current_joypad [addr & 1] & 1) | mic | ext;

		unsigned long result = joypad.joypad_latches [addr & 1];
		joypad.joypad_latches [addr & 1] =
			(result >> 1) | 0x80000000;
		return (result & 1) | mic | ext;
	}
	
	if ( addr == Nes_Apu::status_addr )
		return impl->apu.read_status( clock() );
	return addr >> 8; // simulate open bus
}

// CPU

const int irq_inhibit_mask = 0x04;

nes_addr_t Nes_Core::read_vector( nes_addr_t addr )
{
	uint8_t const* p = cpu::get_code( addr );
	return p [1] * 0x100 + p [0];
}

void Nes_Core::reset( bool full_reset, bool erase_battery_ram )
{
	if ( full_reset )
	{
		cpu::reset( impl->unmapped_page );
		cpu_time_offset = -1;
		clock_ = 0;
		
		// Low RAM
		memset( cpu::low_mem, 0xFF, low_ram_size );
		cpu::low_mem [8] = 0xf7;
		cpu::low_mem [9] = 0xef;
		cpu::low_mem [10] = 0xdf;
		cpu::low_mem [15] = 0xbf;
		
		// SRAM
		lrom_readable = 0;
		sram_present = true;
		sram_bank = 0;
		enable_sram( false );
		if ( !cart->has_battery_ram() || erase_battery_ram )
			memset( impl->sram, 0xFF, impl->sram_size );
		
		joypad.joypad_latches [0] = 0;
		joypad.joypad_latches [1] = 0;
		
		nes.frame_count = 0;
	}
	
	// to do: emulate partial reset
	
	ppu.reset( full_reset );
	impl->apu.reset();
	
	mapper->reset();
	
	cpu::r.pc = read_vector( 0xFFFC );
	cpu::r.sp = 0xfd;
	cpu::r.a = 0;
	cpu::r.x = 0;
	cpu::r.y = 0;
	cpu::r.status = irq_inhibit_mask;
	nes.timestamp = 0;
	error_count = 0;
}

void Nes_Core::vector_interrupt( nes_addr_t vector )
{
	cpu::push_byte( cpu::r.pc >> 8 );
	cpu::push_byte( cpu::r.pc & 0xFF );
	cpu::push_byte( cpu::r.status | 0x20 ); // reserved bit is set
	
	cpu_adjust_time( 7 );
	cpu::r.status |= irq_inhibit_mask;
	cpu::r.pc = read_vector( vector );
}

inline nes_time_t Nes_Core::earliest_irq( nes_time_t present )
{
	return min( impl->apu.earliest_irq( present ), mapper->next_irq( present ) );
}

void Nes_Core::irq_changed()
{
	cpu_set_irq_time( earliest_irq( cpu_time() ) );
}

inline nes_time_t Nes_Core::ppu_frame_length( nes_time_t present )
{
	nes_time_t t = ppu.frame_length();
	if ( t > present )
		return t;
	
	ppu.render_bg_until( clock() ); // to do: why this call to clock() rather than using present?
	return ppu.frame_length();
}

inline nes_time_t Nes_Core::earliest_event( nes_time_t present )
{
	// PPU frame
	nes_time_t t = ppu_frame_length( present );
	
	// DMC
	if ( wait_states_enabled )
		t = min( t, impl->apu.next_dmc_read_time() + 1 );
	
	// NMI
	t = min( t, ppu.nmi_time() );
	
	if ( single_instruction_mode )
		t = min( t, present + 1 );
	
	return t;
}

void Nes_Core::event_changed()
{
	cpu_set_end_time( earliest_event( cpu_time() ) );
}

#undef NES_EMU_CPU_HOOK
#ifndef NES_EMU_CPU_HOOK
	#define NES_EMU_CPU_HOOK( cpu, end_time ) cpu::run( end_time )
#endif

nes_time_t Nes_Core::emulate_frame_()
{
	Nes_Cpu::result_t last_result = cpu::result_cycles;
	int extra_instructions = 0;
	while ( true )
	{
		// Add DMC wait-states to CPU time
		if ( wait_states_enabled )
		{
			impl->apu.run_until( cpu_time() );
			clock_ = cpu_time_offset;
		}
		
		nes_time_t present = cpu_time();
		if ( present >= ppu_frame_length( present ) )
		{
			if ( ppu.nmi_time() <= present )
			{
				// NMI will occur next, so delayed CLI and SEI don't need to be handled.
				// If NMI will occur normally ($2000.7 and $2002.7 set), let it occur
				// next frame, otherwise vector it now.
				
				if ( !(ppu.w2000 & 0x80 & ppu.r2002) )
				{
					/* vectored NMI at end of frame */
					vector_interrupt( 0xFFFA );
					present += 7;
				}
				return present;
			}
			
			if ( extra_instructions > 2 )
			{
				return present;
			}
			
			if ( last_result != cpu::result_cli && last_result != cpu::result_sei &&
					(ppu.nmi_time() >= 0x10000 || (ppu.w2000 & 0x80 & ppu.r2002)) )
				return present;
			
			/* Executing extra instructions for frame */
			extra_instructions++; // execute one more instruction
		}
		
		// NMI
		if ( present >= ppu.nmi_time() )
		{
			ppu.acknowledge_nmi();
			vector_interrupt( 0xFFFA );
			last_result = cpu::result_cycles; // most recent sei/cli won't be delayed now
		}
		
		// IRQ
		nes_time_t irq_time = earliest_irq( present );
		cpu_set_irq_time( irq_time );
		if ( present >= irq_time && (!(cpu::r.status & irq_inhibit_mask) ||
				last_result == cpu::result_sei) )
		{
			if ( last_result != cpu::result_cli )
			{
				/* IRQ vectored */
				mapper->run_until( present );
				vector_interrupt( 0xFFFE );
			}
			else
			{
				// CLI delays IRQ
				cpu_set_irq_time( present + 1 );
			}
		}
		
		// CPU
		nes_time_t end_time = earliest_event( present );
		if ( extra_instructions )
			end_time = present + 1;
		unsigned long cpu_error_count = cpu::error_count();
		last_result = NES_EMU_CPU_HOOK( cpu, end_time - cpu_time_offset - 1 );
		cpu_adjust_time( cpu::time() );
		clock_ = cpu_time_offset;
		error_count += cpu::error_count() - cpu_error_count;
	}
}

nes_time_t Nes_Core::emulate_frame()
{
	joypad_read_count = 0;
	
	cpu_time_offset = ppu.begin_frame( nes.timestamp ) - 1;
	ppu_2002_time = 0;
	clock_ = cpu_time_offset;
	
	// TODO: clean this fucking mess up
	impl->apu.run_until_( emulate_frame_() );
	clock_ = cpu_time_offset;
	impl->apu.run_until_( cpu_time() );
	
	nes_time_t ppu_frame_length = ppu.frame_length();
	nes_time_t length = cpu_time();
	nes.timestamp = ppu.end_frame( length );
	mapper->end_frame( length );
	impl->apu.end_frame( ppu_frame_length );
	
	disable_rendering();
	nes.frame_count++;
	
	return ppu_frame_length;
}

void Nes_Core::add_mapper_intercept( nes_addr_t addr, unsigned size, bool read, bool write )
{
	int end = (addr + size + (page_size - 1)) >> page_bits;
	for ( int page = addr >> page_bits; page < end; page++ )
	{
		data_reader_mapped [page] |= read;
		data_writer_mapped [page] |= write;
	}
}
