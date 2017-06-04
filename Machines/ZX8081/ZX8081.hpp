//
//  ZX8081.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef ZX8081_hpp
#define ZX8081_hpp

#include "../ConfigurationTarget.hpp"
#include "../CRTMachine.hpp"

#include "../../Processors/Z80/Z80.hpp"

namespace ZX8081 {

class Machine:
	public CPU::Z80::Processor<Machine>,
	public CRTMachine::Machine,
	public ConfigurationTarget::Machine {
	public:
		Machine();

		int perform_machine_cycle(const CPU::Z80::MachineCycle &cycle);

		void setup_output(float aspect_ratio);
		void close_output();

		std::shared_ptr<Outputs::CRT::CRT> get_crt();
		std::shared_ptr<Outputs::Speaker> get_speaker();

		void run_for_cycles(int number_of_cycles);

		void configure_as_target(const StaticAnalyser::Target &target);

	private:
		std::shared_ptr<Outputs::CRT::CRT> crt_;
};

}

#endif /* ZX8081_hpp */
