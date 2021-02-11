//
//  ADB.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/10/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef Apple_IIgs_ADB_hpp
#define Apple_IIgs_ADB_hpp

#include <cstdint>
#include <vector>
#include "../../../InstructionSets/M50740/Executor.hpp"
#include "../ADB/Bus.hpp"

namespace Apple {
namespace IIgs {
namespace ADB {

class GLU: public InstructionSet::M50740::PortHandler {
	public:
		GLU();

		uint8_t get_keyboard_data();
		uint8_t get_mouse_data();
		uint8_t get_modifier_status();
		uint8_t get_any_key_down();

		uint8_t get_data();
		uint8_t get_status();

		void set_command(uint8_t);
		void set_status(uint8_t);
		void clear_key_strobe();

		void set_microcontroller_rom(const std::vector<uint8_t> &rom);

		void run_for(Cycles cycles);

	private:
		InstructionSet::M50740::Executor executor_;

		void run_ports_for(Cycles) override;
		void set_port_output(int port, uint8_t value) override;
		uint8_t get_port_input(int port) override;

		uint8_t registers_[16];
		uint8_t register_address_;
		uint8_t register_latch_ = 0xff;

		uint8_t status_ = 0x00;

		Apple::ADB::Bus bus_;
		size_t controller_id_;

		// TODO: add some other devices, and attach them to the ADB bus.
};

}
}
}

#endif /* Apple_IIgs_ADB_hpp */
