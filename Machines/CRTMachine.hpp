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
		Machine() : clock_is_unlimited_(false) {}

		virtual void setup_output(float aspect_ratio) = 0;
		virtual void close_output() = 0;

		virtual std::shared_ptr<Outputs::CRT::CRT> get_crt() = 0;
		virtual std::shared_ptr<Outputs::Speaker> get_speaker() = 0;

		virtual void run_for_cycles(int number_of_cycles) = 0;

		// TODO: sever the clock-rate stuff.
		double get_clock_rate() {
			return clock_rate_;
		}
		bool get_clock_is_unlimited() {
			return clock_is_unlimited_;
		}
		class Delegate {
			public:
				virtual void machine_did_change_clock_rate(Machine *machine) = 0;
				virtual void machine_did_change_clock_is_unlimited(Machine *machine) = 0;
		};
		void set_delegate(Delegate *delegate) { this->delegate_ = delegate; }

	protected:
		void set_clock_rate(double clock_rate) {
			if(clock_rate_ != clock_rate) {
				clock_rate_ = clock_rate;
				if(delegate_) delegate_->machine_did_change_clock_rate(this);
			}
		}
		void set_clock_is_unlimited(bool clock_is_unlimited) {
			if(clock_is_unlimited != clock_is_unlimited_) {
				clock_is_unlimited_ = clock_is_unlimited;
				if(delegate_) delegate_->machine_did_change_clock_is_unlimited(this);
			}
		}

	private:
		Delegate *delegate_;
		double clock_rate_;
		bool clock_is_unlimited_;
};

}

#endif /* CRTMachine_hpp */
