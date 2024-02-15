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
			vias_{ {handlers_[0]}, {handlers_[1]} } {
			set_select_constraints(0);
			handlers_[0].card = handlers_[1].card = this;
		}

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

		void run_for(Cycles cycles, int) final {
			vias_[0].run_for(cycles);
			vias_[1].run_for(cycles);
		}

		bool nmi() final {
			return handlers_[1].interrupt;
		}

		bool irq() final {
			return handlers_[0].interrupt;
		}

		void did_change_interrupt_flags() {
			delegate_->card_did_change_interrupt_flags(this);
		}

	private:
		struct AYVIA: public MOS::MOS6522::PortHandler {
			void set_interrupt_status(bool status) {
				interrupt = status;
				card->did_change_interrupt_flags();
			}

			bool interrupt;
			Mockingboard *card = nullptr;
		};

		MOS::MOS6522::MOS6522<AYVIA> vias_[2];
		AYVIA handlers_[2];
};

}
