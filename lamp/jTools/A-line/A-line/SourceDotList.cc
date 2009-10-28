/*	================
 *	SourceDotList.cc
 *	================
 */

#include "A-line/SourceDotList.hh"

// text-input
#include "text_input/feed.hh"
#include "text_input/get_line_from_feed.hh"

// poseven
#include "poseven/extras/fd_reader.hh"
#include "poseven/functions/open.hh"

// BitsAndBytes
#include "StringPredicates.hh"


namespace tool
{
	
	namespace NN = Nucleus;
	namespace p7 = poseven;
	
	using BitsAndBytes::eos;
	
	
	void ReadSourceDotList( const std::string&           pathname,
	                        std::vector< std::string >&  files )
	{
		text_input::feed feed;
		
		NN::Owned< p7::fd_t > fd = p7::open( pathname, p7::o_rdonly );
		
		p7::fd_reader reader( fd );
		
		while ( const std::string* s = get_line_from_feed( feed, reader ) )
		{
			std::string line( s->begin(), s->end() - 1 );
			
			if ( line.empty()             )  continue;
			if ( line[ 0 ] == ';'         )  continue;
			if ( line[ 0 ] == '#'         )  continue;
			if ( !eos( line.find( ':' ) ) )  continue;
			
			files.push_back( line.substr( line.find_first_not_of( "\t" ),
			                              line.npos ) );
		}
	}
	
	std::vector< std::string > ReadSourceDotList( const std::string& pathname )
	{
		std::vector< std::string > files;
		
		ReadSourceDotList( pathname, files );
		
		return files;
	}
	
}

