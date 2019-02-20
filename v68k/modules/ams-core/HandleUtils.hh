/*
	HandleUtils.hh
	--------------
*/

#ifndef HANDLEUTILS_HH
#define HANDLEUTILS_HH

short HandToHand_patch( char** h : __A0 );

short PtrToHand_patch( char* p : __A0, long size : __D0 );

short PtrToXHand_patch( const void* p : __A0, char** h : __A1, long n : __D0 );

#endif
