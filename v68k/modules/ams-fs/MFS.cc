/*
	MFS.cc
	------
*/

#include "MFS.hh"

// Mac OS
#ifndef __FILES__
#include <Files.h>
#endif
#ifndef __STRINGCOMPARE__
#include <StringCompare.h>
#endif

// ams-common
#include "callouts.hh"


struct logical_block
{
	uint8_t bytes[ 512 ];
};

const mfs::file_directory_entry* MFS_lookup( VCB* vcb, const uint8_t* name )
{
	if ( vcb->vcbSigWord != 0xD2D7 )
	{
		return NULL;
	}
	
	if ( name == NULL  ||  name[ 0 ] == '\0' )
	{
		return NULL;
	}
	
	const logical_block* all_blocks = (logical_block*) vcb->vcbBufAdr;
	
	const logical_block* master_directory_block = all_blocks + 2;
	
	const logical_block* file_directory = all_blocks + vcb->vcbVBMSt;  // drDirSt
	
	typedef const mfs::file_directory_entry* Iter;
	
	Iter it = (Iter) file_directory;
	
	while ( it->flAttrib < 0 )
	{
		if ( EqualString( it->flNam, name, false, true ) )
		{
			return it;
		}
		
		const uint8_t* next = it->flNam + (1 + it->flNam[ 0 ] + 1 & 0xFE);
		
		it = (Iter) next;
	}
	
	return NULL;
}

static
int16_t get_next( const uint8_t* block_map, int16_t curBlk )
{
	if ( curBlk == 1 )
	{
		return 0;
	}
	
	curBlk -= 2;
	
	if ( curBlk < 0 )
	{
		return -1;
	}
	
	block_map += 3 * curBlk / 2;
	
	if ( curBlk & 1 )
	{
		return (block_map[ 0 ] & 0xF) << 8 | block_map[ 1 ];
	}
	else
	{
		return block_map[ 0 ] << 4 | block_map[ 1 ] >> 4;
	}
}

void MFS_load( VCB* vcb, uint16_t stBlk, Ptr buffer, int16_t n )
{
	const logical_block* all_blocks = (logical_block*) vcb->vcbBufAdr;
	
	const logical_block* master_directory_block = all_blocks + 2;
	
	const uint8_t* block_map = (const uint8_t*) master_directory_block + 64;
	
	const char* alloc_blocks = (const char*) (all_blocks + vcb->vcbAlBlSt);
	
	const uint32_t alBlkSiz = vcb->vcbAlBlkSiz;
	
	while ( stBlk > 1  &&  n-- > 0 )
	{
		fast_memcpy( buffer, alloc_blocks + (stBlk - 2) * alBlkSiz, alBlkSiz );
		
		buffer += alBlkSiz;
		
		stBlk = get_next( block_map, stBlk );
	}
}
