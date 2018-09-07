/*
	pack_bits.hh
	------------
*/

#ifndef QUICKDRAW_PACKBITS_HH
#define QUICKDRAW_PACKBITS_HH


namespace quickdraw
{
	
	typedef unsigned char uint8_t;
	
	void unpack_bits( uint8_t const*& src, uint8_t*& dst, unsigned n_dst );
	
}

#endif
