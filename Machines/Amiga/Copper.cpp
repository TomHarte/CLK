//
//  Copper.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/09/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef NDEBUG
#define NDEBUG
#endif

#define LOG_PREFIX "[Copper] "
#include "../../Outputs/Log.hpp"

#include "Chipset.hpp"
#include "Copper.hpp"

using namespace Amiga;

bool Copper::advance_dma(uint16_t position) {
	switch(state_) {
		default: return false;

		case State::Waiting:
			// TODO: blitter-finished bit.
			if((position & position_mask_) >= instruction_[0]) {
				state_ = State::FetchFirstWord;
			}
		return false;

		case State::FetchFirstWord:
			instruction_[0] = ram_[address_ & ram_mask_];
			++address_;
			state_ = State::FetchSecondWord;
		break;

		case State::FetchSecondWord: {
			const bool should_skip_move = skip_next_;
			skip_next_ = false;

			instruction_[1] = ram_[address_ & ram_mask_];
			++address_;

			if(!(instruction_[0] & 1)) {
				// A MOVE.

				if(!should_skip_move) {
					// Stop if this move would be a privilege violation.
					instruction_[0] &= 0x1fe;
					if((instruction_[0] < 0x10) || (instruction_[0] < 0x20 && !(control_&1))) {
						LOG("Invalid MOVE to " << PADHEX(4) << instruction_[0] << "; stopping");
						state_ = State::Stopped;
						break;
					}

					// Construct a 68000-esque Microcycle in order to be able to perform the access.
					CPU::MC68000::Microcycle cycle;
					cycle.operation = CPU::MC68000::Microcycle::SelectWord;
					uint32_t full_address = instruction_[0];
					CPU::RegisterPair16 data = instruction_[1];
					cycle.address = &full_address;
					cycle.value = &data;
					chipset_.perform(cycle);
				}

				// Roll onto the next command.
				state_ = State::FetchFirstWord;
				break;
			}

			// Prepare for a position comparison.
			position_mask_ = 0x8001 | (instruction_[1] & 0x7ffe);
			instruction_[0] &= position_mask_;

			if(!(instruction_[1] & 1)) {
				// A WAIT. Just note that this is now waiting; the proper test
				// will be applied from the next potential `advance` onwards.
				state_ = State::Waiting;
				break;
			}

			// Neither a WAIT nor a MOVE => a SKIP.

			// TODO: blitter-finished bit.
			skip_next_ = (position & position_mask_) >= instruction_[0];
			state_ = State::FetchFirstWord;
		} break;
	}

	return true;
}
