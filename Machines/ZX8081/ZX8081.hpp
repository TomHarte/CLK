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

#include <cstdint>
#include <vector>

namespace ZX8081 {

enum ROMType: uint8_t {
	ZX80, ZX81
};

class Machine:
	public CPU::Z80::Processor<Machine>,
	public CRTMachine::Machine,
	public ConfigurationTarget::Machine {
	public:
		Machine();

		int perform_machine_cycle(const CPU::Z80::MachineCycle &cycle);
		void flush();

		void setup_output(float aspect_ratio);
		void close_output();

		std::shared_ptr<Outputs::CRT::CRT> get_crt();
		std::shared_ptr<Outputs::Speaker> get_speaker();

		void run_for_cycles(int number_of_cycles);

		void configure_as_target(const StaticAnalyser::Target &target);

		void set_rom(ROMType type, std::vector<uint8_t> data);

	private:
		std::shared_ptr<Outputs::CRT::CRT> crt_;
		std::vector<uint8_t> zx81_rom_, zx80_rom_, rom_;
		std::vector<uint8_t> ram_;

		bool vertical_sync_;

		int cycles_since_display_update_;
		void update_display();
		void set_sync(bool sync);
		void output_byte(uint8_t byte);
};

}

#endif /* ZX8081_hpp */
