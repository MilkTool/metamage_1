/*
	instructions.hh
	---------------
*/

#ifndef V68K_INSTRUCTIONS_HH
#define V68K_INSTRUCTIONS_HH

// v68k
#include "v68k/instruction.hh"


namespace v68k
{
	
	extern instruction decoded_MOVEP_to;
	extern instruction decoded_MOVEP_from;
	
	extern instruction decoded_LINK_L;
	
	extern instruction decoded_BKPT;
	
	extern instruction decoded_EXT_W;
	extern instruction decoded_EXT_L;
	
	extern instruction decoded_EXTB;
	
	extern instruction decoded_LINK;
	extern instruction decoded_UNLK;
	
	extern instruction decoded_MOVE_to_USP;
	extern instruction decoded_MOVE_from_USP;
	
	extern instruction decoded_NOP;
	
	extern instruction decoded_STOP;
	
	extern instruction decoded_MOVEQ;
	
	extern instruction decoded_EXG;
	
}

#endif

