/*	==========
 *	DeskBus.cc
 *	==========
 */

#include "ClassicToolbox/DeskBus.hh"

// Mac OS
#ifndef __MACERRORS__
#include <MacErrors.h>
#endif

// Nitrogen
#include "Nitrogen/OSStatus.hh"


namespace Nitrogen
{
	
	// does nothing, but guarantees construction of theRegistration
	NUCLEUS_DEFINE_ERRORS_DEPENDENCY( ADBManager )
	
	
	static void RegisterADBManagerErrors();
	
	
#if NUCLEUS_RICH_ERRORCODES
#pragma force_active on
	
	class ADBManagerErrorsRegistration
	{
		public:
			ADBManagerErrorsRegistration()  { RegisterADBManagerErrors(); }
	};
	
	static ADBManagerErrorsRegistration theRegistration;
	
#pragma force_active reset
#endif
	
	
	void ADBOp( ::Ptr refCon, ADBCompletionUPP completion, ::Ptr buffer, short commandNum )
	{
		Mac::ThrowOSStatus( ::ADBOp( refCon, completion, buffer, commandNum ) );
	}
	
	
	ADBAddress GetIndADB( ADBDataBlock& data, short index )
	{
		::ADBAddress address = ::GetIndADB( &data, index );
		
		if ( address == -1 )
		{
			throw GetIndADB_Failed();
		}
		
		return ADBAddress( address );
	}
	
	ADBDataBlock GetADBInfo( ADBAddress adbAttr )
	{
		ADBDataBlock result;
		
		Mac::ThrowOSStatus( ::GetADBInfo( &result, adbAttr ) );
		
		return result;
	}
	
	void SetADBInfo( const ADBSetInfoBlock& info, ADBAddress adbAttr )
	{
		Mac::ThrowOSStatus( ::SetADBInfo( &info, adbAttr ) );
	}
	
	void RegisterADBManagerErrors()
	{
		//RegisterOSStatus< errADBOp >();  // not defined in MacErrors.h or DeskBus.h
		RegisterOSStatus< qErr >();
	}
	
}
