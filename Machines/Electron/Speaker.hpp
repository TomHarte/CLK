//
//  Speaker.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/12/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Electron_Speaker_hpp
#define Electron_Speaker_hpp

#include "../../Outputs/Speaker.hpp"

namespace Electron {

class Speaker: public ::Outputs::Filter<Speaker> {
	public:
		void set_divider(uint8_t divider);

		void set_is_enabled(bool is_enabled);

		void get_samples(unsigned int number_of_samples, int16_t *target);
		void skip_samples(unsigned int number_of_samples);

		static const unsigned int clock_rate_divider = 8;

	private:
		unsigned int counter_;
		unsigned int divider_;
		bool is_enabled_;
};

}

#endif /* Speaker_hpp */
