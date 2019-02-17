/*
	CopyBits.cc
	-----------
*/

#include "CopyBits.hh"

// Mac OS
#ifndef __QUICKDRAW__
#include <Quickdraw.h>
#endif

// Standard C
#include <stdlib.h>
#include <string.h>

// quickdraw
#include "qd/region_iterator.hh"

// ams-common
#include "callouts.hh"
#include "QDGlobals.hh"
#include "redraw_lock.hh"

// ams-qd
#include "draw.hh"


static inline
bool byte_aligned( short srcSkip, short dstSkip, short width )
{
	return srcSkip + dstSkip == 0  &&  width % 8 == 0;
}

static inline
bool word_aligned( const void* src, const void* dst )
{
	return !( ((uint32_t) src | (uint32_t) dst) & 1 );
}

static
bool equal_bitmaps( const BitMap* a, const BitMap* b )
{
	return a == b  ||  memcmp( a, b, sizeof (BitMap) ) == 0;
}

template < class Ptr >
static
void copy_aligned_sector( Ptr    src,
                          Ptr    dst,
                          short  n_rows,
                          short  n_per_row,
                          short  src_stride,
                          short  dst_stride )
{
	const short n_bytes = n_per_row * sizeof *src;
	
	while ( --n_rows >= 0 )
	{
		fast_memcpy( dst, src, n_bytes );
		
		src += src_stride;
		dst += dst_stride;
	}
}

static
Ptr blit_byte_aligned_segment( Ptr    src,
                               Ptr    dst,
                               short  n_bytes,
                               short  transfer_mode_AND_0x07 )
{
	switch ( transfer_mode_AND_0x07 )
	{
		// Use src vs. pat modes because we stripped off the 8 bit
		
		case srcCopy:
			fast_memcpy( dst, src, n_bytes );
			dst += n_bytes;
			break;
		
		case srcOr:       while ( --n_bytes >= 0 )  *dst++ |=  *src++;  break;
		case srcXor:      while ( --n_bytes >= 0 )  *dst++ ^=  *src++;  break;
		case srcBic:      while ( --n_bytes >= 0 )  *dst++ &= ~*src++;  break;
		case notSrcCopy:  while ( --n_bytes >= 0 )  *dst++  = ~*src++;  break;
		case notSrcOr:    while ( --n_bytes >= 0 )  *dst++ |= ~*src++;  break;
		case notSrcXor:   while ( --n_bytes >= 0 )  *dst++ ^= ~*src++;  break;
		case notSrcBic:   while ( --n_bytes >= 0 )  *dst++ &=  *src++;  break;
	}
	
	return dst;
}

static
void blit_segment_direct( Ptr      src,
                          Ptr      dst,
                          short    n_pixels_skipped,
                          short    n_pixels_drawn,
                          short    transfer_mode_AND_0x07 )
{
	if ( n_pixels_skipped )
	{
		uint8_t mask = (1 << (8 - n_pixels_skipped)) - 1;
		
		n_pixels_drawn -= 8 - n_pixels_skipped;
		
		if ( n_pixels_drawn < 0 )
		{
			n_pixels_skipped = -n_pixels_drawn;
			
			mask &= -(1 << n_pixels_skipped);
			
			n_pixels_drawn = 0;
		}
		
		dst = draw_masked_byte( *src++,
		                        mask,
		                        dst,
		                        transfer_mode_AND_0x07 );
	}
	
	if ( n_pixels_drawn == 0 )
	{
		return;
	}
	
	if ( short n_bytes = n_pixels_drawn / 8 )
	{
		dst = blit_byte_aligned_segment( src,
		                                 dst,
		                                 n_bytes,
		                                 transfer_mode_AND_0x07 );
		
		src += n_bytes;
	}
	
	n_pixels_drawn &= 0x7;
	
	if ( n_pixels_drawn > 0 )
	{
		n_pixels_skipped = 8 - n_pixels_drawn;
		
		const uint8_t mask = -(1 << n_pixels_skipped);
		
		dst = draw_masked_byte( *src++,
		                        mask,
		                        dst,
		                        transfer_mode_AND_0x07 );
	}
}

typedef void (*segment_blitter)(Ptr, Ptr, short, short, uint16_t, short);

static
void blit_segment_buffered( Ptr       src,
                            Ptr       dst,
                            short     n_pixels_skipped,
                            short     dummy,
                            uint16_t  n_pixels_drawn,
                            short     transfer_mode_AND_0x03 )
{
	const short mode = transfer_mode_AND_0x03;
	
	const size_t n_bytes = (n_pixels_skipped + n_pixels_drawn + 7) / 8;
	
	Ptr buffer = (Ptr) alloca( n_bytes );
	
	fast_memcpy( buffer, src, n_bytes );
	
	blit_segment_direct( buffer, dst, n_pixels_skipped, n_pixels_drawn, mode );
}

static
void blit_segment( Ptr       src,
                   Ptr       dst,
                   short     n_src_pixels_skipped,
                   short     n_dst_pixels_skipped,
                   uint16_t  n_pixels_drawn,
                   short     transfer_mode_AND_0x07 )
{
	const short mode = transfer_mode_AND_0x07;
	
	if ( const short skip_delta = n_src_pixels_skipped - n_dst_pixels_skipped )
	{
		const uint8_t left_shift  = skip_delta & 0x7;
		const uint8_t right_shift = 8 - left_shift;
		
		// Theoretical 8K maximum
		const size_t buffer_size = n_pixels_drawn / 8 + 1;
		
		Ptr const buffer = (Ptr) alloca( buffer_size );
		
		buffer[ 0 ] = '\0';
		
		short n_src_bytes = (n_src_pixels_skipped + n_pixels_drawn + 7) / 8;
		
		fast_rshift( buffer, src, n_src_bytes, right_shift );
		
		src = buffer + (skip_delta > 0);
	}
	
	blit_segment_direct( src, dst, n_dst_pixels_skipped, n_pixels_drawn, mode );
}

static
void blit_masked_bits( const BitMap&    srcBits,
                       const BitMap&    dstBits,
                       short            dh,
                       short            dv,
                       short            mode,
                       RgnHandle        maskRgn,
                       segment_blitter  copy_segment )
{
	const short srcVOffset = srcBits.bounds.top  + dv;
	const short srcHOffset = srcBits.bounds.left + dh;
	
	const short dstVOffset = dstBits.bounds.top;
	const short dstHOffset = dstBits.bounds.left;
	
	const short* extent = (short*) (*maskRgn + 1);
	
	quickdraw::region_iterator it( maskRgn[0]->rgnSize, extent );
	
	while ( const quickdraw::region_band* band = it.next() )
	{
		const short v0 = band->v0;
		const short v1 = band->v1;
		
		Ptr p = srcBits.baseAddr + (v0 - srcVOffset) * srcBits.rowBytes;
		Ptr q = dstBits.baseAddr + (v0 - dstVOffset) * dstBits.rowBytes;
		
		for ( short v = v0;  v < v1;  ++v )
		{
			const short* it  = band->h_begin;
			const short* end = band->h_end;
			
			while ( it < end )
			{
				const short h0 = *it++;
				const short h1 = *it++;
				
				Ptr const src = p + (h0 - srcHOffset) / 8;
				Ptr const dst = q + (h0 - dstHOffset) / 8;
				
				const short n_src_pixels_skipped = h0 - srcHOffset & 7;
				const short n_dst_pixels_skipped = h0 - dstHOffset & 7;
				const short n_pixels_drawn       = h1 - h0;
				
				copy_segment( src,
				              dst,
				              n_src_pixels_skipped,
				              n_dst_pixels_skipped,
				              n_pixels_drawn,
				              mode );
			}
			
			p += srcBits.rowBytes;
			q += dstBits.rowBytes;
		}
	}
}

pascal void StdBits_patch( const BitMap*  srcBits,
                           const Rect*    srcRect,
                           const Rect*    dstRect,
                           short          mode,
                           MacRegion**    maskRgn )
{
	const int srcWidth  = srcRect->right - srcRect->left;
	const int srcHeight = srcRect->bottom - srcRect->top;
	
	const int dstWidth  = dstRect->right - dstRect->left;
	const int dstHeight = dstRect->bottom - dstRect->top;
	
	if ( srcWidth <= 0  ||  srcHeight <= 0 )
	{
		return;
	}
	
	if ( dstWidth != srcWidth  ||  dstHeight != srcHeight )
	{
		return;
	}
	
	static RgnHandle clipRgn = NewRgn();
	
	GrafPort& port = **get_addrof_thePort();
	
	redraw_lock lock( port.portBits.baseAddr, srcBits->baseAddr );
	
	get_refined_clip_region( port, *dstRect, clipRgn );
	
	if ( maskRgn )
	{
		SectRgn( maskRgn, clipRgn, clipRgn );
	}
	
	const bool clipping_to_rect = clipRgn[0]->rgnSize <= sizeof (MacRegion);
	
	const Rect clipRect = clipRgn[0]->rgnBBox;
	
	const uint16_t clippedTop  = clipRect.top  - dstRect->top;
	const uint16_t clippedLeft = clipRect.left - dstRect->left;
	
	QDGlobals& qd = get_QDGlobals();
	
	const BitMap* const dstBits = &qd.thePort->portBits;
	
	const short srcTop = srcRect->top + clippedTop - srcBits->bounds.top;
	const short dstTop = dstRect->top + clippedTop - dstBits->bounds.top;
	
	const short srcLeft = srcRect->left + clippedLeft - srcBits->bounds.left;
	const short dstLeft = dstRect->left + clippedLeft - dstBits->bounds.left;
	
	const short srcRowBytes = srcBits->rowBytes;
	const short dstRowBytes = dstBits->rowBytes;
	
	Ptr src = srcBits->baseAddr;
	Ptr dst = dstBits->baseAddr;
	
	src += srcTop * srcRowBytes;
	dst += dstTop * dstRowBytes;
	
	src += srcLeft / 8;
	dst += dstLeft / 8;
	
	const short srcSkip = srcLeft & 0x7;
	const short dstSkip = dstLeft & 0x7;
	
	uint16_t n_rows = clipRect.bottom - clipRect.top;
	uint16_t width  = clipRect.right - clipRect.left;
	
	bool draw_bottom_to_top = false;
	
	segment_blitter blit = &blit_segment;
	
	if ( const bool same_bitmap = srcBits->baseAddr == dstBits->baseAddr )
	{
		if ( const bool moved_to_higher_memory = dst + dstSkip > src + srcSkip )
		{
			if ( const bool vertically_overlapping = dstTop < srcTop + n_rows )
			{
				if ( dstLeft < srcLeft + width  &&  srcLeft < dstLeft + width )
				{
					// Overlapping
					
					if ( dstTop != srcTop )
					{
						draw_bottom_to_top = true;
					}
					else if ( srcSkip == dstSkip )
					{
						blit = &blit_segment_buffered;
					}
				}
			}
		}
	}
	
	if ( clipping_to_rect )
	{
		if ( draw_bottom_to_top )
		{
			src += n_rows * srcRowBytes;
			dst += n_rows * dstRowBytes;
			
			while ( n_rows-- > 0 )
			{
				src -= srcRowBytes;
				dst -= dstRowBytes;
				
				blit_segment( src,
				              dst,
				              srcSkip,
				              dstSkip,
				              width,
				              mode );
			}
		}
		else if ( mode == srcCopy  &&  byte_aligned( srcSkip, dstSkip, width ) )
		{
			width /= 8;
			
			if ( word_aligned( src, dst ) )
			{
				copy_aligned_sector< uint16_t* >( (uint16_t*) src,
				                                  (uint16_t*) dst,
				                                  n_rows,
				                                  width / 2,
				                                  srcRowBytes / 2,
				                                  dstRowBytes / 2 );
				
				if ( (width & 1) == 0 )
				{
					return;
				}
				
				/*
					The buffer addresses are word-aligned but the width isn't
					a multiple of the word size.
				*/
				
				src += width - 1;
				dst += width - 1;
				
				width = 1;
			}
			
			copy_aligned_sector< Ptr >( src,
			                            dst,
			                            n_rows,
			                            width,
			                            srcRowBytes,
			                            dstRowBytes );
		}
		else
		{
			while ( n_rows-- > 0 )
			{
				blit( src,
				      dst,
				      srcSkip,
				      dstSkip,
				      width,
				      mode );
				
				src += srcRowBytes;
				dst += dstRowBytes;
			}
		}
	}
	else
	{
		if ( draw_bottom_to_top )
		{
			const short tmpRight = srcSkip + width;
			const short tmpRowBytes = (tmpRight + 15) / 16 * 2;
			
			Ptr tmp = NewPtr( n_rows * tmpRowBytes );
			
			BitMap tmpBits = { tmp, tmpRowBytes, { 0, 0, n_rows, tmpRight } };
			
			while ( n_rows-- > 0 )
			{
				blit_segment_direct( src,
				                     tmp,
				                     srcSkip,
				                     width,
				                     mode );
				
				src += srcRowBytes;
				tmp += tmpRowBytes;
			}
			
			blit_masked_bits( tmpBits,
			                  *dstBits,
			                  dstLeft - srcSkip,
			                  dstTop  - 0,
			                  mode,
			                  clipRgn,
			                  blit );
			
			DisposePtr( tmpBits.baseAddr );
		}
		else
		{
			blit_masked_bits( *srcBits,
			                  *dstBits,
			                  dstRect->left - srcRect->left,
			                  dstRect->top  - srcRect->top,
			                  mode,
			                  clipRgn,
			                  blit );
		}
	}
}

pascal void CopyBits_patch( const BitMap*  srcBits,
                            const BitMap*  dstBits,
                            const Rect*    srcRect,
                            const Rect*    dstRect,
                            short          mode,
                            MacRegion**    maskRgn )
{
	QDGlobals& qd = get_QDGlobals();
	
	GrafPtr saved_port = NULL;
	
	if ( ! equal_bitmaps( dstBits, &qd.thePort->portBits ) )
	{
		saved_port = qd.thePort;
		
		static GrafPtr port = (GrafPtr) NewPtr( sizeof (GrafPort) );
		
		OpenPort( port );
		
		qd.thePort = port;
		
		SetPortBits( dstBits );
	}
	
	StdBits( srcBits, srcRect, dstRect, mode, maskRgn );
	
	if ( saved_port )
	{
		ClosePort( qd.thePort );
		
		qd.thePort = saved_port;
	}
}

pascal void ScrollRect_patch( const Rect*  srcRect,
                              short        dh,
                              short        dv,
                              MacRegion**  updateRgn )
{
	GrafPtr thePort = *get_addrof_thePort();
	
	Rect dstRect = *srcRect;
	
	OffsetRect( &dstRect, dh, dv );
	
	RectRgn( updateRgn, srcRect );
	
	CopyBits( &thePort->portBits,
	          &thePort->portBits,
	          srcRect,
	          &dstRect,
	          srcCopy,
	          updateRgn );
	
	static RgnHandle tmp = NewRgn();
	
	tmp[0]->rgnBBox = dstRect;
	
	DiffRgn( updateRgn, tmp, updateRgn );
	
	EraseRgn( updateRgn );
}
