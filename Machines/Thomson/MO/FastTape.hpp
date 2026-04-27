//
//  FastTape.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/04/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "MemoryMap.hpp"
#include "Processors/6809/Registers.hpp"

namespace Thomson {
enum class TrapAction {
	None,
	NOP,
	RTS,
};

namespace FastLoader {
struct Loader {
	virtual ~Loader() = default;

	/// @returns: (i) the action to take now with regards to the instruction stream, and (ii) whether to keep the current trap.
	virtual std::pair<TrapAction, bool> did_trap(
		const uint16_t address,
		MemoryAccess &,
		CPU::M6809::Registers &,
		Storage::Tape::BinaryTapePlayer &,
		Storage::Tape::TapeSerialiser &
	) = 0;
};
}
}

#include "FastTapeSchemes/LoricielsBactron.hpp"
#include "FastTapeSchemes/ROM.hpp"

namespace Thomson {
struct FastTapeLoader {
public:
	void reset() {}

	std::optional<uint16_t> trap_address;

	void add_tape_read(const uint16_t address, const MemoryAccess &memory) {
		const auto prior_trap_address = trap_address;

		trap_address = std::nullopt;
		detect<FastLoader::ROM>(address, memory);
		detect<FastLoader::LoricielsBactron>(address, memory);

		if(!trap_address.has_value()) {
			trap_address = prior_trap_address;
		}

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
	}

	TrapAction did_trap(
		const uint16_t address,
		MemoryAccess &memory,
		CPU::M6809::Registers &registers,
		Storage::Tape::BinaryTapePlayer &player,
		Storage::Tape::TapeSerialiser &serialiser
	) {
		if(loader_) {
			const auto result = loader_->did_trap(address, memory, registers, player, serialiser);
			if(!result.second) {
				loader_.reset();
				trap_address = std::nullopt;
			}
			return result.first;
		}

		return TrapAction::None;
	}

private:
	template <typename LoaderT>
	void detect(const uint16_t address, const MemoryAccess &memory) {
		if(trap_address.has_value()) return;

		trap_address = LoaderT::detect(address, memory);
		if(trap_address.has_value()) {
			loader_ = std::make_unique<LoaderT>();
		}
	}

	std::unique_ptr<FastLoader::Loader> loader_ = nullptr;
};


}
