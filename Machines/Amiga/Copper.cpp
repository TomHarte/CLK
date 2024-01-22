//
//  Copper.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/09/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Copper.hpp"
#include "Chipset.hpp"

#include "../../Outputs/Log.hpp"

using namespace Amiga;

namespace {

bool satisfies_raster(uint16_t position, uint16_t blitter_status, uint16_t *instruction) {
	// Return immediately if: (i) wait-for-Blitter is not disabled; and (ii) the Blitter is busy.
	if(!(instruction[1] & 0x8000) && (blitter_status & 0x4000)) {
		return false;
	}

	// Otherwise, test the raster position against the instruction's value and mask.
	const uint16_t mask = 0x8000 | (instruction[1] & 0x7ffe);
	return (position & mask) >= (instruction[0] & mask);
}

Log::Logger<Log::Source::AmigaCopper> logger;

}

//
// Quick notes on the Copper:
//
// There are three instructions: move, wait and skip. All are two words in length.
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
//			b15:	1 => don't also test whether the Blitter is finished; 0 => test.
//
bool Copper::advance_dma(uint16_t position, uint16_t blitter_status) {
	switch(state_) {
		default: return false;

		case State::Waiting:
			if(satisfies_raster(position, blitter_status, instruction_)) {
				logger.info().append("Unblocked waiting for %04x at %04x with mask %04x", instruction_[0], position, instruction_[1] & 0x7ffe);
				state_ = State::FetchFirstWord;
			}
		return false;

		case State::FetchFirstWord:
			instruction_[0] = ram_[address_ & ram_mask_];
			++address_;
			state_ = State::FetchSecondWord;
			logger.info().append("First word fetch at %04x", position);
		break;

		case State::FetchSecondWord: {
			// Get and reset the should-skip-next flag.
			const bool should_skip_move = skip_next_;
			skip_next_ = false;

			// Read in the second instruction word.
			instruction_[1] = ram_[address_ & ram_mask_];
			++address_;
			logger.info().append("Second word fetch at %04x", position);

			// Check for a MOVE.
			if(!(instruction_[0] & 1)) {
				if(!should_skip_move) {
					// Stop if this move would be a privilege violation.
					instruction_[0] &= 0x1fe;
					if((instruction_[0] < 0x10) || (instruction_[0] < 0x20 && !(control_&1))) {
						logger.info().append("Invalid MOVE to %04x; stopping", instruction_[0]);
						state_ = State::Stopped;
						break;
					}

					chipset_.write(instruction_[0], instruction_[1]);
				}

				// Roll onto the next command.
				state_ = State::FetchFirstWord;
				break;
			}

			// Got to here => this is a WAIT or a SKIP.

			if(!(instruction_[1] & 1)) {
				// A WAIT. The wait-for-start-of-next PAL wait of
				// $FFDF,$FFFE seems to suggest evaluation will happen
				// in the next cycle rather than this one.
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
