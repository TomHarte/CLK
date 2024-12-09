//
//  Plus4.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/12/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#include "Plus4.hpp"

#include "../../MachineTypes.hpp"

#include "../../../Analyser/Static/Commodore/Target.hpp"

using namespace Commodore::Plus4;

namespace {

class ConcreteMachine:
	public MachineTypes::TimedMachine,
	public MachineTypes::ScanProducer,
	public MachineTypes::MediaTarget,
	public Machine {
public:
	ConcreteMachine(const Analyser::Static::Commodore::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) {
		const ROM::Request request = ROM::Request(ROM::Name::Plus4BASIC) && ROM::Request(ROM::Name::Plus4KernelPALv5);
		auto roms = rom_fetcher(request);
		if(!request.validate(roms)) {
			throw ROMMachine::Error::MissingROMs;
		}

		insert_media(target.media);
	}

private:
	void set_scan_target(Outputs::Display::ScanTarget *const) final {
	}

	Outputs::Display::ScanStatus get_scaled_scan_status() const final {
		return {};
	}

	void run_for(const Cycles) final {
	}

	bool insert_media(const Analyser::Static::Media &) final {
		return true;
	}
};

}
std::unique_ptr<Machine> Machine::Plus4(
	const Analyser::Static::Target *target,
	const ROMMachine::ROMFetcher &rom_fetcher
) {
	using Target = Analyser::Static::Commodore::Target;
	const Target *const commodore_target = dynamic_cast<const Target *>(target);
	return std::make_unique<ConcreteMachine>(*commodore_target, rom_fetcher);
}
