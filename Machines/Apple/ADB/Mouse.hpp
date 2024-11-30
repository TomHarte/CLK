//
//  Mouse.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/02/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include "ReactiveDevice.hpp"
#include "../../../Inputs/Mouse.hpp"

namespace Apple::ADB {

class Mouse: public ReactiveDevice, public Inputs::Mouse {
	public:
		Mouse(Bus &);

	private:
		void perform_command(const Command &command) override;

		void move(int x, int y) override;
		int get_number_of_buttons() const override;
		void set_button_pressed(int index, bool is_pressed) override;
		void reset_all_buttons() override;

		std::atomic<int16_t> delta_x_, delta_y_;
		std::atomic<int> button_flags_ = 0;
		uint16_t last_posted_reg0_ = 0;
};

}
