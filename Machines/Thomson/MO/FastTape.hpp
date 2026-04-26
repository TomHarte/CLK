//
//  FastTape.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/04/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "MemoryMap.hpp"

#include "FastTapeSchemes/LoricielsBactron.hpp"

//#include "Numeric/CircularCounter.hpp"



//#include <array>

namespace Thomson {

/*!
*/
struct FastTapeLoader {
public:
	void reset() {}

	std::optional<uint16_t> trap_address;

	template <typename MemoryMapT>
	void add_tape_read(const uint16_t address, const MemoryMapT &memory_map) {
		trap_address = std::nullopt;
		detect<FastLoader::LoricielsBactron>(Routine::LoricielsBactron, address, memory_map);

//			const auto matches = [&](std::initializer_list<int> sequence) {
//				auto pointer = tape_access_pointer_;
//				for(const auto value: sequence) {
//					if(tape_access_pcs_[size_t(pointer)] != value) return false;
//					++pointer;
//				}
//				return true;
//			};
//
//			if(matches({0x2d7b, 0x2dcc, 0x2dd3})) {
//				return Routine::MicroidsChicago90GrandPrix500;
//			}
//
//			if(matches({0x3459, 0x3460, 0x3467})) {
//				return Routine::LoricielsMach3;
//			}
//
//			if(matches({0x402b, 0x4032, 0x4039})) {
//				return Routine::LoricielsSpaceRacer;
//			}
//
//			if(matches({0x402b, 0x4032, 0x4039})) {
//				return Routine::LoricielsSpaceRacer;
//			}

			// TODO: check history of PCs and look at code if necessary to figure out
			// which loader this is.

//			printf("[%04x, %04x, %04x, %04x]\n", tape_access_pcs_[0], tape_access_pcs_[1], tape_access_pcs_[2], tape_access_pcs_[3]);
//			return Routine::None;
//		} ();
//
//		if(detected != detected_) {
//			printf("Detected %d\n", detected_);
//			detected_ = detected;
//		}
	}

	void did_trap() const {
	}

private:
	enum class Routine {
		None,
		LoricielsBactron,
	};

	template <typename Loader, typename MemoryMapT>
	void detect(const Routine routine, const uint16_t address, const MemoryMapT &memory_map) {
		if(trap_address.has_value()) return;

		trap_address = Loader::detect(address, memory_map);
		if(trap_address.has_value()) {
			detected_ = routine;
		}
	}

	Routine detected_ = Routine::None;
};


}
