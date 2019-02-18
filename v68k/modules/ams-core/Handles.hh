/*
	Handles.hh
	----------
*/

#ifndef HANDLES_HH
#define HANDLES_HH

char** NewHandle_patch( long size : __D0, short trap_word : __D1 );

char** NewEmptyHandle_patch();

short HLock_patch  ( char** h : __A0 );
short HUnlock_patch( char** h : __A0 );

short HPurge_patch  ( char** h : __A0 );
short HNoPurge_patch( char** h : __A0 );

pascal void MoveHHi_patch( char** h : __A0 );

short DisposeHandle_patch( char** h : __A0 );

short SetHandleSize_patch( char**  h         : __A0,
                           long    size      : __D0,
                           short   trap_word : __D1 );

long GetHandleSize_patch( char** h : __A0 );

short ReallocateHandle_patch( char**  h         : __A0,
                              long    size      : __D0,
                              short   trap_word : __D1 );

short EmptyHandle_patch( char** h : __A0 );

short SetApplLimit_patch( char* p : __A0 );

void MoreMasters_patch();

long FreeMem_patch() : __D0;

void MaxMem_patch();

short ReserveMem_patch( long needed : __D0, short trap_word : __D1 );

short SetGrowZone_patch( void* proc : __A0 );

void MaxApplZone_patch();

signed char HGetState_patch( char** h : __A0 ) : __D0;

short HSetState_patch( char** h : __A0, signed char state : __D0 );

#endif
