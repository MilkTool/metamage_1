/*
	mount.cc
	--------
*/

#include "mount.hh"

// Mac OS
#ifndef __FILES__
#include <Files.h>
#endif

// Standard C
#include <stdlib.h>
#include <string.h>

// ams-fs
#include "freemount.hh"


VCB* DefVCBPtr : 0x0352;
QHdr VCBQHdr   : 0x0356;

void try_to_mount( const char* name )
{
	/*
		Don't save/restore A4 here.  Currently, we're only called before
		module_A4_suspend(), so our A4 is already active (and "restoring" it
		would be harmful).  If/when mounting is allowed later, the new caller
		can save/restore A4 on its own.
	*/
	
	plus::var_string data;
	
	int nerr = try_to_get( name, strlen( name ), data );
	
	if ( nerr )
	{
		return;
	}
	
	const size_t data_size = data.size();
	
	Ptr image = NewPtrSys( data_size );
	
	if ( image == NULL )
	{
		return;
	}
	
	VCB* vcb = (VCB*) malloc( sizeof (VCB) );
	
	if ( vcb == NULL )
	{
		DisposePtr( image );
		return;
	}
	
	BlockMoveData( data.data(), image, data_size );
	
	Ptr master_directory_block = image + 1024;
	
	vcb->qType    = fsQType;
	vcb->vcbFlags = 0;
	
	BlockMoveData( master_directory_block, &vcb->vcbSigWord, 64 );
	
	static short last_vRefNum = -1;  // Reserve -1 for the virtual boot disk.
	
	vcb->vcbVRefNum = --last_vRefNum;
	vcb->vcbMAdr    = NULL;
	vcb->vcbBufAdr  = image;
	vcb->vcbMLen    = 0;
	
	Enqueue( (QElemPtr) vcb, &VCBQHdr );
	
	if ( DefVCBPtr == NULL )
	{
		DefVCBPtr = vcb;
	}
	
	const uint16_t sigword = vcb->vcbSigWord;
	
	if ( sigword == 0xD2D7 )
	{
		vcb->vcbMAdr = master_directory_block + 64;
		vcb->vcbMLen = (vcb->vcbNmAlBlks * 12 + 7) / 8;
	}
}
