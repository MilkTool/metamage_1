// SystemConsole.cc

#include "Genie/SystemConsole.hh"

// Pedestal
#include "Pedestal/Console.hh"
#include "Pedestal/Scroller.hh"
#include "Pedestal/UserWindow.hh"


namespace Genie
{
	
	namespace N = Nitrogen;
	namespace Ped = Pedestal;
	
	
	typedef Ped::Scroller< Ped::Console,
	                       Ped::kLiveFeedbackVariant >
	        SystemConsoleView;
	
	class SystemConsole : public Ped::UserWindow
	{
		public:
			typedef Ped::UserWindow Base;
			
			SystemConsole( const boost::shared_ptr< Ped::WindowCloseHandler >& handler );
	};
	
	class SystemConsoleOwner : public Ped::UniqueWindowOwner< SystemConsole >
	{
		public:
			SystemConsoleOwner()  {}
			
			static int WriteToSystemConsole( const char* data, std::size_t byteCount );
	};
	
	
	static Rect MakeWindowRect()
	{
		BitMap screenBits = N::GetQDGlobalsScreenBits();
		
		Rect rect = N::SetRect(0, 18, 2*4+6*80+16, 18+2*4+11*25+16);
		short mbarHeight = ::GetMBarHeight();
		short vMargin = (screenBits.bounds.bottom - rect.bottom) - mbarHeight;
		short hMargin = (screenBits.bounds.right  - rect.right);
		
		return N::OffsetRect( rect,
		                      hMargin / 2,
		                      mbarHeight + vMargin / 3 );
	}
	
	static inline std::auto_ptr< Ped::View > MakeView()
	{
		return std::auto_ptr< Ped::View >( new SystemConsoleView( MakeWindowRect(), SystemConsoleView::Initializer() ) );
	}
	
	SystemConsole::SystemConsole( const boost::shared_ptr< Ped::WindowCloseHandler >& handler )
	: Base( Ped::NewWindowContext( MakeWindowRect(),
	                               "\p" "System Console" ),
	        N::documentProc )
	{
		SetCloseHandler( handler );
		
		SetView( MakeView() );
	}
	
	int SystemConsoleOwner::WriteToSystemConsole( const char* data, std::size_t byteCount )
	{
		static SystemConsoleOwner gSystemConsoleOwner;
		
		gSystemConsoleOwner.Show();
		
		SystemConsoleView& view = gSystemConsoleOwner.itsWindow->SubView().Get< SystemConsoleView >();
		
		view.ScrolledView().WriteChars( data, byteCount );
		
		view.UpdateScrollbars( N::SetPt( 0, 0 ),
		                       N::SetPt( 0, 0 ) );
		
		return byteCount;
	}
	
	int WriteToSystemConsole( const char* data, std::size_t byteCount )
	{
		return SystemConsoleOwner::WriteToSystemConsole( data, byteCount );
	}
	
}

