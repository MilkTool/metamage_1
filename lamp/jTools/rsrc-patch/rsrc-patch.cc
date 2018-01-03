/*	=============
 *	rsrc-patch.cc
 *	=============
 */

// Standard C++
#include <algorithm>

// Standard C
#include <stdlib.h>
#include <string.h>

// command
#include "command/get_option.hh"

// gear
#include "gear/hexadecimal.hh"
#include "gear/inscribe_decimal.hh"
#include "gear/parse_decimal.hh"
#include "gear/quad.hh"

// plus
#include "plus/var_string.hh"

// poseven
#include "poseven/functions/perror.hh"

// Nitrogen
#include "Mac/Toolbox/Types/OSStatus.hh"
#include "Mac/Toolbox/Utilities/ThrowOSStatus.hh"

#include "Nitrogen/Resources.hh"
#include "Nitrogen/Str.hh"

// Divergence
#include "Divergence/Utilities.hh"

// OSErrno
#include "OSErrno/OSErrno.hh"

// Orion
#include "Orion/Main.hh"


using namespace command::constants;

enum
{
	Option_last_byte = 255,
	
	Option_type,
	Option_id,
	Option_file,
	Option_load,
	Option_seek,
	Option_find,
	Option_find_hex,
	Option_write,
	Option_write_hex,
};

static command::option options[] =
{
	{ "type",      Option_type,      Param_required },
	{ "id",        Option_id,        Param_required },
	{ "file",      Option_file,      Param_required },
	{ "load",      Option_load                       },
	{ "seek",      Option_seek,      Param_required },
	{ "find",      Option_find,      Param_required },
	{ "find-hex",  Option_find_hex,  Param_required },
	{ "write",     Option_write,     Param_required },
	{ "write-hex", Option_write_hex, Param_required },
	
	{ NULL }
};


namespace tool
{
	
	namespace n = nucleus;
	namespace N = Nitrogen;
	namespace p7 = poseven;
	namespace Div = Divergence;
	
	
	static const char* gResFilePathname = NULL;
	
	static n::owned< N::ResFileRefNum > gResFile;
	
	static const char* gResType = NULL;
	static const char* gResID   = NULL;
	
	static N::Handle gHandle;
	
	static std::size_t gOffset = 0;
	
	
	// E.g. "666f6f20626171" -> "foo bar"
	static plus::string decoded_hex( const char* hex_codes )
	{
		plus::var_string result;
		
		// FIXME:  Verify the hex data.
		
		char* p = result.reset( strlen( hex_codes ) / 2 );
		
		for ( std::size_t i = 0;  i < result.size();  ++i )
		{
			const char high = hex_codes[ i * 2     ];
			const char low  = hex_codes[ i * 2 + 1 ];
			
			p[ i ] = gear::decoded_hex_digit( high ) << 4
			       | gear::decoded_hex_digit( low  );
		}
		
		return result;
	}
	
	
	static void FileOptor( const char* param )
	{
		gResFilePathname = param;
		
		gResFile.reset();  // next line resets only if resolve and open succeed
		
		gResFile = N::FSpOpenResFile( Div::ResolvePathToFSSpec( param ), Mac::fsRdWrPerm );
	}
	
	static void LoadOptor( const char* )
	{
		gOffset = 0;
		
		if ( gHandle )
		{
			N::ReleaseResource( gHandle );
		}
		
		if ( !gResFile.get() )
		{
			Mac::ThrowOSStatus( resFNotFound );
		}
		
		if ( gResType == NULL  ||  gResID == NULL )
		{
			Mac::ThrowOSStatus( resNotFound );
		}
		
		if ( strlen( gResType ) != sizeof (::ResType) )
		{
			Mac::ThrowOSStatus( paramErr );
		}
		
		N::ResType resType = N::ResType( gear::decode_quad( gResType ) );
		
		N::ResID   resID   = N::ResID( gear::parse_decimal( gResID ) );
		
		gHandle = N::Get1Resource( resType, resID );
	}
	
	static void Find( const plus::string& pattern )
	{
		const char* begin = *gHandle.Get();
		const char* end   = begin + N::GetHandleSize( gHandle );
		
		const char* mark = std::search( begin + gOffset,
		                                end,
		                                pattern.begin(),
		                                pattern.end() );
		
		if ( mark == end )
		{
			Mac::ThrowOSStatus( paramErr );
		}
		
		gOffset = mark - begin;
	}
	
	static void FindOptor( const char* param )
	{
		Find( param );
	}
	
	static void FindHexOptor( const char* param )
	{
		Find( decoded_hex( param ) );
	}
	
	static void SeekOptor( const char* param )
	{
		std::size_t delta = gear::parse_unsigned_decimal( param );
		
		gOffset += delta;
	}
	
	static void Write( const plus::string& bytes )
	{
		if ( gOffset + bytes.size() > N::GetHandleSize( gHandle ) )
		{
			Mac::ThrowOSStatus( paramErr );
		}
		
		char* begin = *gHandle.Get();
		
		std::copy( bytes.begin(), bytes.end(), begin + gOffset );
		
		gOffset += bytes.size();
		
		N::ChangedResource( gHandle );
	}
	
	static void WriteOptor( const char* param )
	{
		Write( param );
	}
	
	static void WriteHexOptor( const char* param )
	{
		Write( decoded_hex( param ) );
	}
	
}

static char* const* get_options( char* const* argv )
{
	++argv;  // skip arg 0
	
	short opt;
	
	while ( (opt = command::get_option( &argv, options )) )
	{
		using namespace tool;
		
		switch ( opt )
		{
			case Option_type:
				gResType = command::global_result.param;
				break;
			
			case Option_id:
				gResID = command::global_result.param;
				break;
			
			case Option_file:
				FileOptor( command::global_result.param );
				break;
			
			case Option_load:
				LoadOptor( command::global_result.param );
				break;
			
			case Option_seek:
				SeekOptor( command::global_result.param );
				break;
			
			case Option_find:
				FindOptor( command::global_result.param );
				break;
			
			case Option_find_hex:
				FindHexOptor( command::global_result.param );
				break;
			
			case Option_write:
				WriteOptor( command::global_result.param );
				break;
			
			case Option_write_hex:
				WriteHexOptor( command::global_result.param );
				break;
			
			default:
				abort();
		}
	}
	
	return argv;
}

namespace tool
{
	
	int Main( int argc, char** argv )
	{
		try
		{
			char *const *args = get_options( argv );
			
			const int argn = argc - (args - argv);
		}
		catch ( const Mac::OSStatus& err )
		{
			plus::var_string status = "OSStatus ";
			
			status += gear::inscribe_decimal( err );
			
			p7::perror( "rsrc-patch", status, 0 );
			
			p7::throw_errno( OSErrno::ErrnoFromOSStatus( err ) );
		}
		
		return 0;
	}

}
