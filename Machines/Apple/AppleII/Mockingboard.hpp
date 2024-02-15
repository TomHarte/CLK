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

class AYPair {
	public:
		AYPair(Concurrency::AsyncTaskQueue<false> &queue) :
			ays_{
				{GI::AY38910::Personality::AY38910, queue},
				{GI::AY38910::Personality::AY38910, queue},
			} {}

		void advance() {
			ays_[0].advance();
			ays_[1].advance();
		}

		void set_sample_volume_range(std::int16_t range) {
			ays_[0].set_sample_volume_range(range >> 1);
			ays_[1].set_sample_volume_range(range >> 1);
		}

		bool is_zero_level() const {
			return ays_[0].is_zero_level() && ays_[1].is_zero_level();
		}

		Outputs::Speaker::MonoSample level() const {
			return ays_[0].level() + ays_[1].level();
		}

		GI::AY38910::AY38910SampleSource<false> &get(int index) {
			return ays_[index];
		}

	private:
		GI::AY38910::AY38910SampleSource<false> ays_[2];
};

class Mockingboard: public Card {
	public:
		Mockingboard(AYPair &ays) :
			vias_{ {handlers_[0]}, {handlers_[1]} },
			handlers_{ {*this, ays.get(0)}, {*this, ays.get(1)}} {
			set_select_constraints(0);
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
			AYVIA(Mockingboard &card, GI::AY38910::AY38910SampleSource<false> &ay) :
				card(card), ay(ay) {}

			void set_interrupt_status(bool status) {
				interrupt = status;
				card.did_change_interrupt_flags();
			}

			void set_port_output(MOS::MOS6522::Port port, uint8_t value, uint8_t) {
				if(port) {
					using ControlLines = GI::AY38910::ControlLines;
					ay.set_control_lines(
						ControlLines(
							((value & 1) ? ControlLines::BC1 : 0) |
							((value & 2) ? ControlLines::BDIR : 0) |
							((value & 4) ? ControlLines::BC2 : 0)
						)
					);

					// TODO: all lines disabled sees to map to reset? Possibly?
					// Cf. https://gswv.apple2.org.za/a2zine/Docs/Mockingboard_MiniManual.html
				} else {
					ay.set_data_input(value);
				}
			}

			uint8_t get_port_input(MOS::MOS6522::Port port) {
				if(!port) {
					return ay.get_data_output();
				}
				return 0xff;
			}

			bool interrupt;
			Mockingboard &card;
			GI::AY38910::AY38910SampleSource<false> &ay;
		};

		MOS::MOS6522::MOS6522<AYVIA> vias_[2];
		AYVIA handlers_[2];
};

}
