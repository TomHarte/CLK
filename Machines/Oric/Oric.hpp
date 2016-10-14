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
#include "../../Components/6522/6522.hpp"
#include "../../Storage/Tape/Tape.hpp"

#include "../ConfigurationTarget.hpp"
#include "../CRTMachine.hpp"
#include "../Typer.hpp"

#include "Video.hpp"

#include <cstdint>
#include <vector>

namespace Oric {

class VIA: public MOS::MOS6522<VIA>, public MOS::MOS6522IRQDelegate {
	public:
		using MOS6522IRQDelegate::set_interrupt_status;

		void set_port_output(Port port, uint8_t value, uint8_t direction_mask) {
			port_outputs[port] = value;
			if(port)
				set_control_line_input(port, Line::One, (value << 1)&value&128);
		}
		uint8_t get_port_input(Port port) {
			if(port)
			{
				return port_outputs[0];
			}
			else
				return (uint8_t)((port_outputs[port] >> 4) | (port_outputs[port] << 4));
		}
		uint8_t port_outputs[2];
};

class Machine:
	public CPU6502::Processor<Machine>,
	public CRTMachine::Machine,
	public ConfigurationTarget::Machine,
	public MOS::MOS6522IRQDelegate::Delegate {

	public:
		Machine();

		void set_rom(std::vector<uint8_t> data);

		// to satisfy ConfigurationTarget::Machine
		void configure_as_target(const StaticAnalyser::Target &target);

		// to satisfy CPU6502::Processor
		unsigned int perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value);
		void synchronise();

		// to satisfy CRTMachine::Machine
		virtual void setup_output(float aspect_ratio);
		virtual void close_output();
		virtual std::shared_ptr<Outputs::CRT::CRT> get_crt() { return _videoOutput->get_crt(); }
		virtual std::shared_ptr<Outputs::Speaker> get_speaker() { return nullptr; }
		virtual void run_for_cycles(int number_of_cycles) { CPU6502::Processor<Machine>::run_for_cycles(number_of_cycles); }

		// to satisfy MOS::MOS6522IRQDelegate::Delegate
		void mos6522_did_change_interrupt_status(void *mos6522);

	private:
		// RAM and ROM
		uint8_t _ram[65536], _rom[16384];
		int _cycles_since_video_update;
		inline void update_video();

		// Outputs
		std::unique_ptr<VideoOutput> _videoOutput;

		//
		VIA _via;
};

}
#endif /* Oric_hpp */
