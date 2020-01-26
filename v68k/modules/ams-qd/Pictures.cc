/*
	Pictures.cc
	-----------
*/

#include "Pictures.hh"

// Mac OS
#ifndef __QUICKDRAW__
#include <Quickdraw.h>
#endif

// log-of-war
#include "logofwar/report.hh"

// quickdraw
#include "qd/pack_bits.hh"

// ams-common
#include "callouts.hh"
#include "QDGlobals.hh"


union Word
{
	SInt16 s16;
	UInt8  u8[2];
};

static
short read_word( const UInt8*& p )
{
	Word result;
	
	result.u8[ 0 ] = *p++;
	result.u8[ 1 ] = *p++;
	
	return result.s16;
}

static
Rect read_Rect( const UInt8*& p )
{
	Rect result;
	
	result.top    = read_word( p );
	result.left   = read_word( p );
	result.bottom = read_word( p );
	result.right  = read_word( p );
	
	return result;
}

static
const UInt8* pen_pat( const UInt8* p )
{
	Pattern pattern;
	fast_memcpy( &pattern, p, sizeof pattern );
	
	PenPat( &pattern );
	
	return p + 8;
}

static
const UInt8* short_line( const UInt8* p, const Rect& target, const Rect& frame )
{
	const short v = read_word( p );
	const short h = read_word( p );
	
	const SInt8 dh = *p++;
	const SInt8 dv = *p++;
	
	MoveTo( h + target.left - frame.left, v + target.top - frame.top );
	
	Line( dh, dv );
	
	return p;
}

static
const UInt8* draw_bits( const UInt8* p, const Rect& target, const Rect& frame )
{
	BitMap bitmap;
	
	const short rowBytes = read_word( p );
	
	bitmap.rowBytes = rowBytes;
	
	bitmap.bounds = read_Rect( p );
	
	Rect srcRect = read_Rect( p );
	Rect dstRect = read_Rect( p );
	
	const short mode = read_word( p );
	
	OffsetRect( &dstRect, target.left - frame.left, target.top - frame.top );
	
	short n_rows = bitmap.bounds.bottom - bitmap.bounds.top;
	
	if ( rowBytes < 8 )
	{
		bitmap.baseAddr = (Ptr) p;
		
		p += rowBytes * n_rows;
		
		StdBits( &bitmap, &srcRect, &dstRect, mode, NULL );
		
		return p;
	}
	
	bitmap.bounds.top    = 0;
	bitmap.bounds.bottom = 1;
	
	srcRect.top    = 0;
	srcRect.bottom = 1;
	
	dstRect.bottom = dstRect.top;
	
	if ( rowBytes <= 250 )
	{
		UInt8 buffer[ 256 ];
		
		bitmap.baseAddr = (Ptr) buffer;
		
		do
		{
			UInt8 count = *p++;
			
			UInt8* dst = buffer;
			
			quickdraw::unpack_bits( p, dst, rowBytes );
			
			++dstRect.bottom;
			
			StdBits( &bitmap, &srcRect, &dstRect, mode, NULL );
			
			++dstRect.top;
		}
		while ( --n_rows > 0 );
	}
	
	return p;
}

static
const Byte* do_opcode( const Byte* p, const Rect& dstRect, const Rect& frame )
{
	const Byte opcode = *p++;
	
	switch ( opcode )
	{
		case 0x00:
		case 0x1E:  // DefHilite
			break;
		
		case 0xA0:
			p += 2;
			break;
		
		case 0x01:
			p += read_word( p ) - 2;
			break;
		
		case 0x09:
			p = pen_pat( p );
			break;
		
		case 0x22:
			p = short_line( p, dstRect, frame );
			break;
		
		case 0x90:
		case 0x98:
			p = draw_bits( p, dstRect, frame );
			break;
		
		case 0xFF:
		default:
			return NULL;
	}
	
	return p;
}

static
const Byte* do_opcode2( const Byte* p, const Rect& dstRect, const Rect& frame )
{
	const UInt16* p2 = (const UInt16*) p;
	
	if ( *p++ == 0 )
	{
		p = do_opcode( p, dstRect, frame );
		
		if ( (long) p & 1 )
		{
			++p;
		}
		
		return p;
	}
	
	const UInt16 opcode = *p2++;
	
	switch ( opcode )
	{
		case 0x0C00:
			// HeaderOp
			p2 += 24 / 2u;
			break;
		
		default:
			return NULL;
	}
	
	return (const Byte*) p2;
}

pascal void DrawPicture_patch( PicHandle pic, const Rect* dstRect )
{
	GrafPort& port = **get_addrof_thePort();
	
	Size size = GetHandleSize( (Handle) pic );
	
	if ( size < sizeof (Picture) + 4 )
	{
		return;  // $1101FF is valid, but empty
	}
	
	const UInt8* end = (UInt8*) pic[0] + size;
	const UInt8* p   = (UInt8*) (pic[0] + 1);
	
	PenState penState;
	GetPenState( &penState );
	
	port.pnMode = patCopy;
	
	UInt8 c = *p++;
	
	if ( c == 0 )
	{
		c = *p++;
	}
	
	if ( c != 0x11 )
	{
		return;
	}
	
	uint8_t version = *p++;
	
	if ( version == 1 )
	{
		while ( p  &&  p < end )
		{
			p = do_opcode( p, *dstRect, pic[0]->picFrame );
		}
	}
	else if ( version == 2 )
	{
		++p;  // skip $FF after $02
		
		while ( p  &&  p < end )
		{
			p = do_opcode2( p, *dstRect, pic[0]->picFrame );
		}
	}
	else
	{
		WARNING = "Unknown PICT format ", version;
	}
	
	SetPenState( &penState );
}
