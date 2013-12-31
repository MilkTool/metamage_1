/*	============
 *	Directory.cc
 *	============
 */

#include "Genie/IO/Directory.hh"

// Standard C/C++
#include <cstring>

// POSIX
#include "fcntl.h"
#include "sys/stat.h"

// poseven
#include "poseven/types/errno_t.hh"

// vfs
#include "vfs/dir_contents.hh"
#include "vfs/dir_entry.hh"
#include "vfs/node.hh"
#include "vfs/primitives/inode.hh"
#include "vfs/primitives/listdir.hh"
#include "vfs/primitives/parent_inode.hh"

// Genie
#include "Genie/FS/FSTree.hh"


namespace Genie
{
	
	namespace p7 = poseven;
	
	
	static vfs::dir_contents_box get_contents( const vfs::node& dir )
	{
		vfs::dir_contents_box result;
		
		vfs::dir_contents& contents = *result;
		
		contents.push_back( vfs::dir_entry( inode       ( dir ), "."  ) );
		contents.push_back( vfs::dir_entry( parent_inode( dir ), ".." ) );
		
		listdir( dir, contents );
		
		return result;
	}
	
	DirHandle::DirHandle()
	:
		IOHandle( O_RDONLY | O_DIRECTORY )
	{
	}
	
	DirHandle::DirHandle( const vfs::node* dir )
	:
		IOHandle( dir, O_RDONLY | O_DIRECTORY )
	{
	}
	
	DirHandle::~DirHandle()
	{
	}
	
	static void SetDirEntry( dirent& dir, ino_t inode, const plus::string& name )
	{
		dir.d_ino = inode;
		
		std::strcpy( dir.d_name, name.c_str() );  // FIXME:  Unsafe!
	}
	
	int DirHandle::ReadDir( dirent& entry )
	{
		if ( !its_contents.get() )
		{
			its_contents = get_contents( *GetFile() );
		}
		
		vfs::dir_contents& contents = *its_contents;
		
		const int i = get_mark();
		
		if ( i >= contents.size() )
		{
			return 0;
		}
		
		vfs::dir_entry node = contents.at( i );
		
		advance_mark( 1 );
		
		SetDirEntry( entry, node.inode, node.name );
		
		return sizeof (dirent);
	}
	
}

