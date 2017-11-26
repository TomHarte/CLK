//
//  9918.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef _918_hpp
#define _918_hpp

#include "../../Outputs/CRT/CRT.hpp"
#include "../../ClockReceiver/ClockReceiver.hpp"

#include <cstdint>

namespace TI {

class TMS9918 {
	public:
		enum Personality {
			TMS9918A,	// includes the 9928A
		};

		/*!
			Constructs an instance of the drive controller that behaves according to personality @c p.
			@param p The type of controller to emulate.
		*/
		TMS9918(Personality p);

		std::shared_ptr<Outputs::CRT::CRT> get_crt();

		/*!
			Runs the VCP for the number of cycles indicate; it is an implicit assumption of the code
			that the input clock rate is 3579545 Hz — the NTSC colour clock rate.
		*/
		void run_for(const Cycles cycles);

		void set_register(int address, uint8_t value);
		uint8_t get_register(int address);

	private:
		std::shared_ptr<Outputs::CRT::CRT> crt_;
};

};

#endif /* _918_hpp */
