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

namespace {

bool satisfies_raster(uint16_t position, uint16_t blitter_status, uint16_t *instruction) {
	const uint16_t mask = 0x8000 | (instruction[1] & 0x7ffe);
	return
		(position & mask) >= (instruction[0] & mask)
		&& (!(blitter_status & 0x4000) || (instruction[1] & 0x8000));
}

}

//
// Quick notes on the Copper:
//
// There are three instructions: move, wait and skip.
//
// Move writes a value to one of the Chipset registers; it is encoded as:
//
//		First word:
//			b0:		0
//			b1–b8:	register address
//			b9+:	unused ("should be set to 0")
//
//		Second word:
//			b0–b15:	value to move.
//
//
// Wait waits until the raster gets to at least a certain position, and
// optionally until the Blitter has finished. It is encoded as:
//
//		First word:
//			b0:		1
//			b1–b7:	horizontal beam position
//			b8+:	vertical beam position
//
//		Second word:
//			b0:		0
//			b1–b7:	horizontal beam comparison mask
//			b8–b14:	vertical beam comparison mask
//			b15:	1 => don't also wait for the Blitter to be finished; 0 => wait.
//
//
// Skip skips the next instruction if the raster has already reached a certain
// position, and optionally only if the Blitter has finished, and only if the
// next instruction is a move.
//
//		First word:
//			b0:		1
//			b1–b7:	horizontal beam position
//			b8+:	vertical beam position
//
//		Second word:
//			b0:		1
//			b1–b7:	horizontal beam comparison mask
//			b8–b14:	vertical beam comparison mask
//			b15:	1 => don't also wait for the Blitter to be finished; 0 => wait.
//
bool Copper::advance_dma(uint16_t position, uint16_t blitter_status) {
	switch(state_) {
		default: return false;

		case State::Waiting:
			if(satisfies_raster(position, blitter_status, instruction_)) {
				LOG("Unblocked waiting for " << PADHEX(4) << instruction_[0] << " at " << position);
				state_ = State::FetchFirstWord;
			}
		return false;

		case State::FetchFirstWord:
			instruction_[0] = ram_[address_ & ram_mask_];
			++address_;
			state_ = State::FetchSecondWord;
		break;

		case State::FetchSecondWord: {
			// Get and reset the should-skip-next flag.
			const bool should_skip_move = skip_next_;
			skip_next_ = false;

			// Read in the second instruction word.
			instruction_[1] = ram_[address_ & ram_mask_];
			++address_;

			// Check for a MOVE.
			if(!(instruction_[0] & 1)) {
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

			// Got to here => this is a WAIT or a SKIP.

			if(!(instruction_[1] & 1)) {
				// A WAIT. Just note that this is now waiting; the proper test
				// will be applied from the next potential `advance_dma` onwards.
				state_ = State::Waiting;
				break;
			}

			// Neither a WAIT nor a MOVE => a SKIP.

			skip_next_ = satisfies_raster(position, blitter_status, instruction_);
			state_ = State::FetchFirstWord;
		} break;
	}

	return true;
}
