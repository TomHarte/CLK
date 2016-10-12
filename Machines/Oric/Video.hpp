//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Video_hpp
#define Video_hpp

#include "../../Outputs/CRT/CRT.hpp"

namespace Oric {

class VideoOutput {
	public:
		VideoOutput(uint8_t *memory);
		void set_crt(std::shared_ptr<Outputs::CRT::CRT> crt);
		void run_for_cycles(int number_of_cycles);

	private:
		uint8_t *_ram;
};

}

#endif /* Video_hpp */
