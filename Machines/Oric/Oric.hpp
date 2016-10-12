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

#include "Video.hpp"

#include <cstdint>
#include <vector>

namespace Oric {

class Machine:
	public CPU6502::Processor<Machine>,
	public CRTMachine::Machine,
	public ConfigurationTarget::Machine {

	public:
		Machine();

		void set_rom(std::vector<uint8_t> data);

		// to satisfy ConfigurationTarget::Machine
		void configure_as_target(const StaticAnalyser::Target &target);

		// to satisfy CPU6502::Processor
		unsigned int perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value);
		void synchronise() { update_video(); }

		// to satisfy CRTMachine::Machine
		virtual void setup_output(float aspect_ratio);
		virtual void close_output();
		virtual std::shared_ptr<Outputs::CRT::CRT> get_crt() { return _crt; }
		virtual std::shared_ptr<Outputs::Speaker> get_speaker() { return nullptr; }
		virtual void run_for_cycles(int number_of_cycles) { CPU6502::Processor<Machine>::run_for_cycles(number_of_cycles); }

	private:
		// RAM and ROM
		uint8_t _ram[65536], _rom[16384];
		int _cycles_since_video_update;
		inline void update_video();

		// Outputs
		std::shared_ptr<Outputs::CRT::CRT> _crt;
		std::unique_ptr<VideoOutput> _videoOutput;
};

}
#endif /* Oric_hpp */
