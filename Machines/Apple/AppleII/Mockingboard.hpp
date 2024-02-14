//
//  Mockingboard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/02/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "Card.hpp"

#include "../../../Components/6522/6522.hpp"


namespace Apple::II {

class Mockingboard: public Card {
	public:
		Mockingboard() :
			vias_{ {handlers_[0]}, {handlers_[1]} } {}

		void perform_bus_operation(Select select, bool is_read, uint16_t address, uint8_t *value) final {
			if(!(select & Device)) {
				return;
			}

			int index = (address >> 7) & 1;
			if(is_read) {
				*value = vias_[index].read(address);
			} else {
				vias_[index].write(address, *value);
			}
		}

	private:
		class AYVIA: public MOS::MOS6522::IRQDelegatePortHandler {
		};

		MOS::MOS6522::MOS6522<AYVIA> vias_[2];
		AYVIA handlers_[2];
};

}
