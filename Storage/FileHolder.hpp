//
//  FileHolder.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef FileHolder_hpp
#define FileHolder_hpp

#include <sys/stat.h>
#include <cstdio>

namespace Storage {

class FileHolder {
	public:
		enum {
			ErrorCantOpen = -1
		};

		virtual ~FileHolder();

	protected:
		FileHolder(const char *file_name);

		FILE *file_;
		struct stat file_stats_;
};

}

#endif /* FileHolder_hpp */
