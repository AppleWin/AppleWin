/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2014, Tom Charlesworth, Michael Pohoreski

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Description: Add ProDOS File System support for formatting disk images
 * Author: Michael Pohoreski
 */

// --- ProDOS Consts ---

	const size_t PRODOS_BLOCK_SIZE   = 0x200; // 512 bytes/block
	const size_t PRODOS_ROOT_BLOCK   = 2;
	const int    PRODOS_ROOT_OFFSET  = PRODOS_ROOT_BLOCK * PRODOS_BLOCK_SIZE;

	const int    PRODOS_MAX_FILENAME = 15;
	const int    PRODOS_MAX_PATH     = 64; // TODO: Verify

	const uint8_t ACCESS_D = 0x80; // Can destroy
	const uint8_t ACCESS_N = 0x40; // Can rename 
	const uint8_t ACCESS_B = 0x20; // Can backup
	const uint8_t ACCESS_Z4= 0x10; // not used - must be zero?
	const uint8_t ACCESS_Z3= 0x08;
	const uint8_t ACCESS_I = 0x04; // invisible
	const uint8_t ACCESS_W = 0x02; // Can write
	const uint8_t ACCESS_R = 0x01; // Can read

    enum PRODOS_Kind_e
    {
        PRODOS_KIND_DEL  = 0x0,
        PRODOS_KIND_SEED = 0x1, // Single Block
        PRODOS_KIND_SAPL = 0x2, // 1st Block is allocated blocks
        PRODOS_KIND_TREE = 0x3,
        PRODOS_KIND_PAS  = 0x4, // http://www.1000bit.it/support/manuali/apple/technotes/pdos/tn.pdos.25.html
        PRODOS_KIND_GSOS = 0x5, // http://www.1000bit.it/support/manuali/apple/technotes/pdos/tn.pdos.25.html
        PRODOS_KIND_DIR  = 0xD, // parent block entry for sub-directory
        PRODOS_KIND_SUB  = 0xE, // subdir header entry to find parent directory; meta to reach parent
        PRODOS_KIND_ROOT = 0xF, // volume header entry for root directory
        NUM_PRODOS_KIND  = 16
    };

// --- ProDOS Structs ---
	struct ProDOS_SubDirInfo_t
	{
		uint8_t res75  ; // $75 - magic number
		uint8_t pad7[7];
	};

	struct ProDOS_VolumeExtra_t
	{
		uint16_t bitmap_block;
		uint16_t total_blocks;
	};

	struct ProDOS_SubDirExtra_t
	{
		uint16_t parent_block;
		uint8_t  parent_entry_num;
		uint8_t  parent_entry_len;
};

	// Due to packing/aignment this may NOT be size identical
	// But it is in the correct order
	// See: ProDOS_GetVolumeHeader;
	struct ProDOS_VolumeHeader_t
	{                                  ; //Rel Size Abs
		uint8_t  kind                  ; // +0    1 $04  \ Hi nibble  Storage Type
		uint8_t  len                   ; // +0           / Lo nibble
		char     name[ 16 ]            ; // +1   15 $05  15 on disk but we NULL terminate for convenience
	// --- diff from file ---
		union
		{
			uint8_t             pad8[8]; //+16    8 $14 only in root
			ProDOS_SubDirInfo_t  info  ; //             only in sub-dir
		}                              ;
	// --- same as file ---
		uint16_t date                  ; //+24    2 $1C
		uint16_t time                  ; //+26    2 $1E
		uint8_t  cur_ver               ; //+28    1 $20
		uint8_t  min_ver               ; //+29    1 $21
		uint8_t  access                ; //+30    1 $22
		uint8_t  entry_len             ; //+31    1 $23 Size of each directory entry in bytes
		uint8_t  entry_num             ; //+32    1 $24 Number of directory entries per block
		uint16_t file_count            ; //+33    2 $25 Active entries including directories, excludes volume header
		union                            //          --
		{                                //          --
			ProDOS_VolumeExtra_t meta  ; //+34    2 $27
			ProDOS_SubDirExtra_t subdir; //+36    2 $29
		}                              ; //============
	}                                  ; // 38      $2B

	struct ProDOS_FileHeader_t
	{                                  ; //Rel Size Hex
		uint8_t  kind                  ; // +0    1 $00  \ Hi nibble Storage Type
		uint8_t  len                   ; // +0           / Lo nibble Filename Length
		char     name[ 16 ]            ; // +1   15 $05  15 on disk but we NULL terminate for convenience
// --- diff from volume ---		       
		uint8_t  type                  ; //+16    1 $10              User Type
		uint16_t inode                 ; //+17    2 $11
		uint16_t blocks                ; //+19    2 $13
		uint32_t size                  ; //+21    3 $15 EOF address - on disk is 3 bytes, but 32-bit for convenience
// --- same as volume ---		       
		uint16_t date                  ; //+24    2 $18
		uint16_t time                  ; //+26    2 $1A
		uint8_t  cur_ver               ; //+28    1 $1C
		uint8_t  min_ver               ; //+29    1 $1D // 0 = ProDOS 1.0
		uint8_t  access                ; //+30    1 $1E
// --- diff from subdir			       
		uint16_t aux                   ; //+31    2 $1F Load Address for Binary
// --- diff from volume ---		       
		uint16_t mod_date              ; //+33    2 $21
		uint16_t mod_time              ; //+35    2 $23
		uint16_t dir_block             ; //+37    2 $25 pointer to directory block
								       ; //============
	};                                 ; // 39      $27

// --- ProDOS Functions ---

	// Helpers for 8 bit, 16-bit, 24-bit values

	// ------------------------------------------------------------------------
	inline uint16_t ProDOS_Get16( uint8_t *pDiskBytes, int offset )
	{
		uint16_t n = 0
		|   (((uint16_t)pDiskBytes[ offset + 0 ]) << 0)
		|   (((uint16_t)pDiskBytes[ offset + 1 ]) << 8)
		;
		return n;
	}

	// ------------------------------------------------------------------------
	inline uint32_t ProDOS_Get24( uint8_t *pDiskBytes, int offset )
	{
		uint32_t n = 0
		|   (((uint32_t)pDiskBytes[ offset + 0 ]) <<  0)
		|   (((uint32_t)pDiskBytes[ offset + 1 ]) <<  8)
		|   (((uint32_t)pDiskBytes[ offset + 2 ]) << 16)
		;
		return n;
	}

	// ------------------------------------------------------------------------
	inline void ProDOS_Put16( uint8_t *pDiskBytes, int offset, int val )
	{
		pDiskBytes[ offset + 0 ] = (val >> 0) & 0xFF;
		pDiskBytes[ offset + 1 ] = (val >> 8) & 0xFF;
	}

	// ------------------------------------------------------------------------
	inline void ProDOS_Put24( uint8_t *pDiskBytes, int offset, int val )
	{
		pDiskBytes[ offset + 0 ] = (val >> 0) & 0xFF;
		pDiskBytes[ offset + 1 ] = (val >> 8) & 0xFF;
		pDiskBytes[ offset + 2 ] = (val >>16) & 0xFF;
	}

	// ============================================================

	// ------------------------------------------------------------------------
	int ProDOS_BlockGetFirstFree( uint8_t *pDiskBytes, size_t nDiskSize, ProDOS_VolumeHeader_t *pVolume )
	{
		if( !pVolume )
		{
			return 0;
		}

		int bitmap = pVolume->meta.bitmap_block;
		int offset = bitmap * PRODOS_BLOCK_SIZE;
		int size   = (nDiskSize + 7) / 8;
		int block  = 0;

		for( int byte = 0; byte < size; byte++ )
		{
			int mask = 0x80;
			do
			{
				if( pDiskBytes[ offset + byte ] & mask )
					return block;

				mask >>= 1;
				block++;
			}
			while( mask );
		}

		return 0;
	}

	// ------------------------------------------------------------------------
	bool ProDOS_BlockSetUsed( uint8_t *pDiskBytes, ProDOS_VolumeHeader_t *pVolume, int block )
	{
		if( !pVolume )
		{
			return false;
		}

		int bitmap = pVolume->meta.bitmap_block;
		int offset = bitmap * PRODOS_BLOCK_SIZE;

		int byte = block / 8;
		int bits = block % 8;
		int mask = 0x80 >> bits;

		pDiskBytes[ offset + byte ] |= mask;
		pDiskBytes[ offset + byte ] ^= mask;

		return true;
	}

	// ------------------------------------------------------------
	void ProDOS_GetFileHeader( uint8_t *pDiskBytes, int offset, ProDOS_FileHeader_t *file_ )
	{
		ProDOS_FileHeader_t info;

		info.kind       =              (pDiskBytes[ offset + 0  ] >> 4) & 0xF;
		info.len        =              (pDiskBytes[ offset + 0  ] >> 0) & 0xF;

		for( int i = 0; i < 16; i++ )
			info.name[ i ] = 0;

		for( int i = 0; i < info.len; i++ )
			info.name[ i ]  =           pDiskBytes[ offset + 1+i];

		info.type       =               pDiskBytes[ offset + 16 ];
		info.inode      = ProDOS_Get16( pDiskBytes, offset + 17 );
		info.blocks     = ProDOS_Get16( pDiskBytes, offset + 19 );
		info.size       = ProDOS_Get24( pDiskBytes, offset + 21 );
		info.date       = ProDOS_Get16( pDiskBytes, offset + 24 );
		info.time       = ProDOS_Get16( pDiskBytes, offset + 26 );
		info.cur_ver    =               pDiskBytes[ offset + 28 ];
		info.min_ver    =               pDiskBytes[ offset + 29 ];
		info.access     =               pDiskBytes[ offset + 30 ];
		info.aux        = ProDOS_Get16( pDiskBytes, offset + 31 );
		info.mod_date   = ProDOS_Get16( pDiskBytes, offset + 33 );
		info.mod_time   = ProDOS_Get16( pDiskBytes, offset + 35 );
		info.dir_block  = ProDOS_Get16( pDiskBytes, offset + 37 );

		if(  file_ )
			*file_ = info;
	}

	// ------------------------------------------------------------------------
	int ProDOS_DirGetFirstFree( uint8_t *pDiskBytes, ProDOS_VolumeHeader_t *pVolume, int base )
	{
		int next_block;
		int prev_block;

		int offset = base + 4;

		// Try to find the file in this directory
		do
		{
			prev_block = ProDOS_Get16( pDiskBytes, base + 0 );
			next_block = ProDOS_Get16( pDiskBytes, base + 2 );

			for( int iFile = 0; iFile < pVolume->entry_num; iFile++ )
			{
				ProDOS_FileHeader_t file;
				ProDOS_GetFileHeader( pDiskBytes, offset, &file );

				if( file.kind == PRODOS_KIND_DEL)
					return offset;

				if( file.len == 0 )
					return offset;

				offset += pVolume->entry_len;
			}

			base   = next_block * PRODOS_BLOCK_SIZE;
			offset = base + 4;

		} while( next_block );

		return 0;
	}
	// ------------------------------------------------------------------------
	inline int ProDOS_GetIndexBlock( uint8_t *pDiskBytes, int offset, int index )
	{
		int block = 0
		| (pDiskBytes[ offset + index + 0   ] << 0)
		| (pDiskBytes[ offset + index + 256 ] << 8)
		;
		return block;
	}

	// ------------------------------------------------------------------------
	int ProDOS_BlockInitFree( uint8_t *pDiskBytes, size_t nDiskSize, ProDOS_VolumeHeader_t *volume )
	{
		int bitmap = volume->meta.bitmap_block;
		int offset = bitmap * PRODOS_BLOCK_SIZE;
		int blocks = (nDiskSize + PRODOS_BLOCK_SIZE - 1) / PRODOS_BLOCK_SIZE;
		int size   = (blocks + 7) / 8;

		memset( &pDiskBytes[ offset ], 0xFF, size );

		volume->meta.total_blocks = blocks;
		return (size + PRODOS_BLOCK_SIZE - 1) / PRODOS_BLOCK_SIZE;
	}

	/*
		000:lo0 lo1 lo2 ...
		100:hi0 hi1 hi2 ...
	*/
	// ------------------------------------------------------------------------
	inline void ProDOS_PutIndexBlock( uint8_t *pDiskBytes, int offset, int index, int block )
	{
		pDiskBytes[ offset + index + 0   ] = (block >> 0) & 0xFF;
		pDiskBytes[ offset + index + 256 ] = (block >> 8) & 0xFF;
	}

	// ------------------------------------------------------------------------
	void ProDOS_SetVolumeHeader(  uint8_t *pDiskBytes, ProDOS_VolumeHeader_t *volume, int block )
	{
		if( !volume )
			return;

		int base = block*PRODOS_BLOCK_SIZE + 4; // skip prev/next dir block double linked list

		uint8_t KindLen = 0
			| ((volume->kind & 0xF) << 4)
			| ((volume->len  & 0xF) << 0)
			;

		pDiskBytes[ base + 0 ] = KindLen;

	for( int i = 0; i < volume->len; i++ )
		pDiskBytes[ base+ 1+i ] = volume->name[ i ];

	for( int i = 0; i < 8; i++ )
		pDiskBytes[ base+16+i ] = volume->pad8[ i ];

		ProDOS_Put16( pDiskBytes, base + 24 ,   volume->date              );
		ProDOS_Put16( pDiskBytes, base + 26 ,   volume->time              );
		              pDiskBytes[ base + 28 ] = volume->cur_ver  ;
		              pDiskBytes[ base + 29 ] = volume->min_ver  ;
		              pDiskBytes[ base + 30 ] = volume->access   ;
		              pDiskBytes[ base + 31 ] = volume->entry_len;
		              pDiskBytes[ base + 32 ] = volume->entry_num; 
		ProDOS_Put16( pDiskBytes, base + 33 ,   volume->file_count        );
		ProDOS_Put16( pDiskBytes, base + 35 ,   volume->meta.bitmap_block );
		ProDOS_Put16( pDiskBytes, base + 37 ,   volume->meta.total_blocks );
	}

	// ------------------------------------------------------------------------
	size_t ProDOS_String_CopyUpper( char *pDst, const char *pSrc, int nLen = 0 )
	{
		char *pBeg = pDst;
		if( !nLen )
			nLen = strlen( pSrc );

		for( int i = 0; i < nLen; i++ )
		{
			char c = *pSrc++;

			if( c == 0 ) // Don't copy past end-of-string
				break;

			if((c >= 'a') && (c <= 'z' ))
				c -= ('a' - 'A');

			*pDst++ = c;
		}

		*pDst = 0;
		return (pDst - pBeg); 
	}
