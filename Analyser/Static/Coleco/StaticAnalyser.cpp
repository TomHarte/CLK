//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/02/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

static std::vector<std::shared_ptr<Storage::Cartridge::Cartridge>>
		ColecoCartridgesFrom(const std::vector<std::shared_ptr<Storage::Cartridge::Cartridge>> &cartridges) {
	std::vector<std::shared_ptr<Storage::Cartridge::Cartridge>> coleco_cartridges;

	for(const auto &cartridge : cartridges) {
		const auto &segments = cartridge->get_segments();

		// only one mapped item is allowed
		if(segments.size() != 1) continue;

		// which must be 8, 12, 16, 24 or 32 kb in size
		const Storage::Cartridge::Cartridge::Segment &segment = segments.front();
		const std::size_t data_size = segment.data.size();
		if((data_size&8191) && (data_size != 12*1024)) continue;
		if(data_size < 8192 || data_size > 32768) continue;

		// the two first bytes must be 0xaa and 0x55, either way around
		if(segment.data[0] != 0xaa && segment.data[0] != 0x55 && segment.data[1] != 0xaa && segment.data[1] != 0x55) continue;
		if(segment.data[0] == segment.data[1]) continue;

		// probability of a random binary blob that isn't a Coleco ROM proceeding to here is 1 - 1/32768.
		coleco_cartridges.push_back(cartridge);
	}

	return coleco_cartridges;
}

void Analyser::Static::Coleco::AddTargets(const Media &media, std::vector<std::unique_ptr<Target>> &destination) {
	std::unique_ptr<Target> target(new Target);
	target->machine = Machine::ColecoVision;
	target->confidence = 0.5;
	target->media.cartridges = ColecoCartridgesFrom(media.cartridges);
	if(!target->media.empty())
		destination.push_back(std::move(target));
}
