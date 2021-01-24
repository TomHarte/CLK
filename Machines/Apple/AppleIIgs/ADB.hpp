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

namespace Apple {
namespace IIgs {
namespace ADB {

class GLU: public InstructionSet::M50740::PortHandler {
	public:
		GLU();

		// Behaviour varies slightly between the controller shipped with ROM01 machines
		// and that shipped with ROM03 machines; use this to set the desired behaviour.
		void set_is_rom03(bool);

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
		bool is_rom03_ = false;
		std::vector<uint8_t> next_command_;
		std::vector<uint8_t> pending_response_;

		void abort();
		void reset_microcontroller();
		void flush_keyboard_buffer();

		void set_modes(uint8_t modes);
		uint8_t modes_ = 0;

		void set_device_srq(int mask);
		int device_srq_ = 0;

		void set_configuration_bytes(uint8_t *);

		uint8_t read_microcontroller_address(uint16_t);

		InstructionSet::M50740::Executor executor_;

		void set_port_output(int port, uint8_t value) override;
		uint8_t get_port_input(int port) override;

		uint8_t registers_[16];
		uint8_t register_address_;
};

}
}
}

#endif /* Apple_IIgs_ADB_hpp */
