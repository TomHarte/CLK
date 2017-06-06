//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef Machines_ZX8081_Video_hpp
#define Machines_ZX8081_Video_hpp

#include "../../Outputs/CRT/CRT.hpp"

namespace ZX8081 {

class Video {
	public:
		Video();

		std::shared_ptr<Outputs::CRT::CRT> get_crt();
		void run_for_cycles(int number_of_cycles);
		void flush();

		void set_sync(bool sync);
		void output_byte(uint8_t byte);

	private:
		bool sync_;
		uint8_t *line_data_, *line_data_pointer_;
		unsigned int cycles_since_update_;
		std::shared_ptr<Outputs::CRT::CRT> crt_;
};

}

#endif /* Video_hpp */
