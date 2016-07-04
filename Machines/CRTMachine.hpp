//
//  CRTMachine.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/05/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef CRTMachine_hpp
#define CRTMachine_hpp

#include "../Outputs/CRT/CRT.hpp"
#include "../Outputs/Speaker.hpp"

namespace CRTMachine {

/*!
	A CRTMachine::Machine is a mostly-abstract base class for machines that connect to a CRT,
	that optionally provide a speaker, and that nominate a clock rate and can announce to a delegate
	should that clock rate change.
*/
class Machine {
	public:
		virtual void setup_output(float aspect_ratio) = 0;
		virtual void close_output() = 0;

		virtual std::shared_ptr<Outputs::CRT::CRT> get_crt() = 0;
		virtual std::shared_ptr<Outputs::Speaker> get_speaker() = 0;

		virtual void run_for_cycles(int number_of_cycles) = 0;

		// TODO: sever the clock-rate stuff.
		virtual double get_clock_rate() = 0;
		class Delegate {
			public:
				virtual void machine_did_change_clock_rate(Machine *machine) = 0;
		};
		void set_delegate(Delegate *delegate) { this->delegate = delegate; }

	protected:
		Delegate *delegate;
};

}

#endif /* CRTMachine_hpp */
