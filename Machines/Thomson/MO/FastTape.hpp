//
//  FastTape.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/04/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "MemoryMap.hpp"

#include "Numeric/CircularCounter.hpp"
#include <array>

namespace Thomson {

struct FastTapeLoader {
public:
	enum class Routine {
		None,
		LoricielsBactronMGT,
		MicroidsChicago90,
		LoricielsMach3,
	};

	Routine detected() const {
		return detected_;
	}

	template <typename MemoryMapT>
	void add_tape_read(const uint16_t address, [[maybe_unused]] const MemoryMapT &memory_map) {
		tape_access_pcs_[static_cast<size_t>(tape_access_pointer_)] = address;
		++tape_access_pointer_;

		const Routine detected = [&] {
			const auto matches = [&](std::initializer_list<int> sequence) {
				auto pointer = tape_access_pointer_;
				for(const auto value: sequence) {
					if(tape_access_pcs_[size_t(pointer)] != value) return false;
					++pointer;
				}
				return true;
			};

			// Test for Loriciels Bactron/MGT.
			if(matches({0x357b, 0x35cc, 0x35d3})) {
				return Routine::LoricielsBactronMGT;
			}

			if(matches({0x2d7b, 0x2dcc, 0x2dd3})) {
				return Routine::MicroidsChicago90;
			}

			if(matches({0x3459, 0x3460, 0x3467})) {
				return Routine::LoricielsMach3;
			}

			// TODO: check history of PCs and look at code if necessary to figure out
			// which loader this is.

			printf("[%04x, %04x, %04x, %04x]\n", tape_access_pcs_[0], tape_access_pcs_[1], tape_access_pcs_[2], tape_access_pcs_[3]);
			return Routine::None;
		} ();

		if(detected != detected_) {
			printf("Detected %d\n", detected_);
			detected_ = detected;
		}
	}

private:
	std::array<uint16_t, 4> tape_access_pcs_;
	Numeric::CircularCounter<int, 4> tape_access_pointer_;
	Routine detected_ = Routine::None;
};


}
