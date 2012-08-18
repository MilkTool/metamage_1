/*	==========
 *	Conduit.cc
 *	==========
 */

#include "Genie/IO/Conduit.hh"

// Standard C
#include <errno.h>
#include <signal.h>

// Standard C++
#include <algorithm>

// Debug
#include "debug/assert.hh"

// poseven
#include "poseven/types/errno_t.hh"

// Genie
#include "Genie/api/signals.hh"
#include "Genie/api/yield.hh"


namespace Genie
{
	
	namespace p7 = poseven;
	
	
	page::page( const page& other )
	:
		n_written( other.n_written ),
		n_read   ( other.n_read    )
	{
		ASSERT( n_read    <= n_written );
		ASSERT( n_written <= capacity  );
		
		std::copy( &other.data[ n_read    ],
		           &other.data[ n_written ],
		           &      data[ n_read    ] );
	}
	
	void page::write( const char* buffer, std::size_t n_bytes )
	{
		ASSERT( n_bytes <= n_writable() );
		
		std::copy( buffer, buffer + n_bytes, &data[ n_written ] );
		
		n_written += n_bytes;
	}
	
	std::size_t page::read( char* buffer, std::size_t max_bytes )
	{
		max_bytes = std::min( max_bytes, n_readable() );
		
		const char* start = &data[ n_read ];
		
		std::copy( start, start + max_bytes, buffer );
		
		n_read += max_bytes;
		
		return max_bytes;
	}
	
	
	bool conduit::is_readable() const
	{
		return its_ingress_has_closed || !its_pages.empty();
	}
	
	bool conduit::is_writable() const
	{
		return its_egress_has_closed || its_pages.size() < 20;
	}
	
	int conduit::read( char* buffer, std::size_t max_bytes, bool nonblocking )
	{
		if ( max_bytes == 0 )
		{
			return 0;
		}
		
		// Wait until we have some data or the stream is closed
		while ( its_pages.empty() && !its_ingress_has_closed )
		{
			try_again( nonblocking );
		}
		
		// Either a page was written, or input was closed,
		// or possibly both, so check its_pages rather than its_ingress_has_closed
		// so we don't miss data.
		
		// If the page queue is still empty then input must have closed.
		if ( its_pages.empty() )
		{
			return 0;
		}
		
		// Only reached if a page is available.
		
		const std::size_t readable = its_pages.front().n_readable();
		
		ASSERT( readable > 0 );
		
		const bool consumed = max_bytes >= readable;
		
		its_pages.front().read( buffer, max_bytes );
		
		if ( consumed )
		{
			its_pages.pop_front();
			
			return readable;
		}
		
		return max_bytes;
	}
	
	int conduit::write( const char* buffer, std::size_t n_bytes, bool nonblocking )
	{
		while ( !is_writable() )
		{
			try_again( nonblocking );
		}
		
		if ( its_egress_has_closed )
		{
			send_signal_to_current_process( SIGPIPE );
			
			p7::throw_errno( EPIPE );
		}
		
		if ( n_bytes == 0 )
		{
			return 0;
		}
		
		if ( its_pages.empty() )
		{
			its_pages.push_back( page() );
		}
		else if ( n_bytes > its_pages.back().n_writable()  &&  n_bytes <= page::capacity )
		{
			its_pages.push_back( page() );
			
			its_pages.back().write( buffer, n_bytes );
			
			return n_bytes;
		}
		
		const char* end = buffer + n_bytes;
		
		std::size_t writable = 0;
		
		while ( end - buffer > (writable = its_pages.back().n_writable()) )
		{
			its_pages.back().write( buffer, writable );
			
			buffer += writable;
			
			its_pages.push_back( page() );
		}
		
		its_pages.back().write( buffer, end - buffer );
		
		
		return n_bytes;
	}
	
}

