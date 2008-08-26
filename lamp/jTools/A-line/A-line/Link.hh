/*	=======
 *	Link.hh
 *	=======
 */

#pragma once

// Standard C++
#include <string>
#include <vector>

// A-line
#include "A-line/Task.hh"


namespace ALine
{
	
	class Project;
	struct TargetInfo;
	
	std::size_t NameObjectFiles( const Project&               project,
	                             std::vector< std::string >&  object_pathnames );
	
	void LinkProduct( const Project&                 project,
	                  const TargetInfo&              targetInfo,
	                  const TaskPtr&                 source_dependency,
	                  const std::vector< TaskPtr >&  tool_dependencies );
	
}

