//
//  Keyboard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Keyboard_hpp
#define Keyboard_hpp

#include <cstdint>

namespace Amiga {

class Keyboard {
	public:
		enum Lines: uint8_t {
			Data = (1 << 0),
			Clock = (1 << 1),
		};

		uint8_t update(uint8_t);

	private:
		enum class ShiftState {
			Shifting,
			AwaitingHandshake,
			Idle,
		} shift_state_ = ShiftState::Idle;

		enum class State {
			Startup,
		} state_ = State::Startup;

		int bit_phase_ = 0;
		uint32_t shift_sequence_ = 0;
		int bits_remaining_ = 0;

		uint8_t lines_ = 0;
};

}

#endif /* Keyboard_hpp */
