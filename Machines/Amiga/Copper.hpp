//
//  Copper.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/09/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Copper_h
#define Copper_h

#include "DMADevice.hpp"

namespace Amiga {

class Copper: public DMADevice<2> {
	public:
		using DMADevice<2>::DMADevice;

		/// Offers a DMA slot to the Copper, specifying the current beam position and Blitter status.
		///
		/// @returns @c true if the slot was used; @c false otherwise.
		bool advance_dma(uint16_t position, uint16_t blitter_status);

		/// Forces a reload of address @c id (i.e. 0 or 1) and restarts the Copper.
		template <int id> void reload() {
			address_ = pointer_[id];
			state_ = State::FetchFirstWord;
		}

		/// Sets the Copper control word.
		void set_control(uint16_t c) {
			control_ = c;
		}

		/// Forces the Copper into the stopped state.
		void stop() {
			state_ = State::Stopped;
		}

	private:
		uint32_t address_ = 0;
		uint16_t control_ = 0;

		enum class State {
			FetchFirstWord, FetchSecondWord, Waiting, Stopped,
		} state_ = State::Stopped;
		bool skip_next_ = false;
		uint16_t instruction_[2]{};
};

}

#endif /* Copper_h */
