//
//  TapeUEF.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef TapeUEF_hpp
#define TapeUEF_hpp

#include "../Tape.hpp"

class UEF : public Storage::Tape {
	public:
		UEF(const char *file_name);
		Cycle get_next_cycle();

	private:
};

#endif /* TapeUEF_hpp */
