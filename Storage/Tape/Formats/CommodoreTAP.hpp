//
//  CommodoreTAP.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef CommodoreTAP_hpp
#define CommodoreTAP_hpp

#include "../Tape.hpp"
#include <stdint.h>

namespace Storage {

class CommodoreTAP: public Tape {
	public:
		CommodoreTAP(const char *file_name);
		~CommodoreTAP();

		Pulse get_next_pulse();
		void reset();

		enum {
			ErrorNotCommodoreTAP
		};

	private:
		FILE *_file;
		bool _updated_layout;
		uint32_t _file_size;

		Pulse _current_pulse;
};

}

#endif /* CommodoreTAP_hpp */
