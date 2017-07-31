//
//  AmstradCPC.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef AmstradCPC_hpp
#define AmstradCPC_hpp

#include "../ConfigurationTarget.hpp"
#include "../CRTMachine.hpp"

#include "../../Processors/Z80/Z80.hpp"
#include "../../Components/AY38910/AY38910.hpp"

namespace AmstradCPC {

class Machine:
	public CPU::Z80::Processor<Machine>,
	public CRTMachine::Machine,
	public ConfigurationTarget::Machine {
	public:
		HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle);
		void flush();

		void setup_output(float aspect_ratio);
		void close_output();

		std::shared_ptr<Outputs::CRT::CRT> get_crt();
		std::shared_ptr<Outputs::Speaker> get_speaker();

		void run_for(const Cycles cycles);

		void configure_as_target(const StaticAnalyser::Target &target);

	private:
		std::shared_ptr<Outputs::CRT::CRT> crt_;
};

}

#endif /* AmstradCPC_hpp */
