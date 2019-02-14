/*
	Fonts.hh
	--------
*/

#ifndef FONTS_HH
#define FONTS_HH

struct FMInput;
struct FMOutput;

pascal void InitFonts_patch();

pascal void GetFNum_patch( const unsigned char* name, short* num );

pascal FMOutput* FMSwapFont_patch( const FMInput* input );

#endif
