while ( true )
{
	while ( count-- )
	{
		// vN211-212: split uses screen-space tile X, not scrolled NT X.
		const bool in_split=mmc5_split_tile(split_col) && mmc5_split_nt;
		const int split_y=mmc5_split_y(mmc5_render_y);
		const int tile_fine_y=in_split ? (split_y&7) : fine_y;
		unsigned long offset;
		cache_t const* lines;
		if (in_split)
		{
			const int col=split_col&31;
			const int nt_off=((split_y>>3)*32+col);
			const int attr_idx=0x3c0+((split_y>>5)*8)+(col>>2);
			const unsigned at=mmc5_split_nt[attr_idx&0x3ff];
			const unsigned shift=((split_y>>4)&2)*2+((col>>1)&1)*2;
			offset=((at>>shift)&3)*attrib_factor+this->palette_offset;
			lines=this->get_bg_tile_split(mmc5_split_nt[nt_off&0x3ff]);
		}
		else
		{
#ifdef BG_EXGRAFIX
			int const exbyte = exgrafix_exram [addr & 0x3ff];
			offset = (exbyte >> 6 & 3) * attrib_factor + this->palette_offset;
			lines = this->get_bg_tile_ex( exbyte, nametable [addr] );
#else
			int attrib = attr_table [addr >> 2 & 0x07];
			attrib >>= (addr >> 4 & 4) | (addr & 2);
			offset = (attrib & 3) * attrib_factor + this->palette_offset;
#ifdef BG_PEC586
			lines = this->get_bg_tile_pec586( nametable [addr], addr );
#else
			lines = this->get_bg_tile( nametable [addr] + bg_bank );
#endif
#endif
		}
		uint8_t* p = pixels;
		addr++;
		++split_col;
		pixels += 8; // next tile
		
		if ( !clipped )
		{
			// optimal case: no clipping
			for ( int n = 4; n--; )
			{
				unsigned long line = *lines++;
				((unaligned_uint32_t*) p) [0].val = (line >> 4 & mask) + offset;
				((unaligned_uint32_t*) p) [1].val = (line      & mask) + offset;
				p += row_bytes;
				((unaligned_uint32_t*) p) [0].val = (line >> 6 & mask) + offset;
				((unaligned_uint32_t*) p) [1].val = (line >> 2 & mask) + offset;
				p += row_bytes;
			}
		}
		else
		{
			lines += tile_fine_y >> 1;
			
			if ( tile_fine_y & 1 )
			{
				unsigned long line = *lines++;
				((unaligned_uint32_t*) p) [0].val = (line >> 6 & mask) + offset;
				((unaligned_uint32_t*) p) [1].val = (line >> 2 & mask) + offset;
				p += row_bytes;
			}
			
			for ( int n = height >> 1; n--; )
			{
				unsigned long line = *lines++;
				((unaligned_uint32_t*) p) [0].val = (line >> 4 & mask) + offset;
				((unaligned_uint32_t*) p) [1].val = (line      & mask) + offset;
				p += row_bytes;
				((unaligned_uint32_t*) p) [0].val = (line >> 6 & mask) + offset;
				((unaligned_uint32_t*) p) [1].val = (line >> 2 & mask) + offset;
				p += row_bytes;
			}
			
			if ( height & 1 )
			{
				unsigned long line = *lines;
				((unaligned_uint32_t*) p) [0].val = (line >> 4 & mask) + offset;
				((unaligned_uint32_t*) p) [1].val = (line      & mask) + offset;
			}
		} 
	}
	
	count = count2;
	count2 = 0;
	addr -= 32;
	attr_table = attr_table - nametable + nametable2;
	/* AURORA_FINAL10_RENDER_VRAM_HOOK_V1 */
	this->notify_vram_address( 0 );
	this->notify_vram_address( 0x2000 | ((mapper_nt_addr ^ 0x400) & 0x0fff) );
	nametable = nametable2;
	if ( !count )
		break;
}
