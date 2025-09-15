//
//  BBCMicro.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/09/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include "BBCMicro.hpp"

#include "Analyser/Static/Acorn/Target.hpp"

namespace BBCMicro {

class ConcreteMachine:
	public Machine
{
public:
	ConcreteMachine(
		const Analyser::Static::Acorn::BBCMicroTarget &target,
		const ROMMachine::ROMFetcher &rom_fetcher
	) {
		(void)target;
		(void)rom_fetcher;
	}
};

}

using namespace BBCMicro;

std::unique_ptr<Machine> Machine::BBCMicro(
	const Analyser::Static::Target *target,
	const ROMMachine::ROMFetcher &rom_fetcher
) {
	using Target = Analyser::Static::Acorn::BBCMicroTarget;
	const Target *const acorn_target = dynamic_cast<const Target *>(target);
	return std::make_unique<BBCMicro::ConcreteMachine>(*acorn_target, rom_fetcher);
}
