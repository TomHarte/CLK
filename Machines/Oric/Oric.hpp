//
//  Oric.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Oric_hpp
#define Oric_hpp

#include "../../Processors/6502/CPU6502.hpp"
#include "../../Storage/Tape/Tape.hpp"

#include "../ConfigurationTarget.hpp"
#include "../CRTMachine.hpp"
#include "../Typer.hpp"

#include <cstdint>
#include <vector>

namespace Oric {

class Machine:
	public CPU6502::Processor<Machine>,
	public CRTMachine::Machine,
	public ConfigurationTarget::Machine {

	public:
		Machine();

		// to satisfy ConfigurationTarget::Machine
		void configure_as_target(const StaticAnalyser::Target &target);

		// to satisfy CPU6502::Processor
		unsigned int perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value);
//		void synchronise();

		// to satisfy CRTMachine::Machine
		virtual void setup_output(float aspect_ratio);
		virtual void close_output();
		virtual std::shared_ptr<Outputs::CRT::CRT> get_crt() { return _crt; }
		virtual std::shared_ptr<Outputs::Speaker> get_speaker() { return nullptr; }
		virtual void run_for_cycles(int number_of_cycles) { CPU6502::Processor<Machine>::run_for_cycles(number_of_cycles); }

	private:
		// Outputs
		std::shared_ptr<Outputs::CRT::CRT> _crt;
};

}
#endif /* Oric_hpp */
