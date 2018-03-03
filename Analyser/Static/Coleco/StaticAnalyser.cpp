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
		const std::size_t overflow = data_size&8191;
		if(overflow > 8 && overflow != 512 && (data_size != 12*1024)) continue;
		if(data_size < 8192) continue;

		// the two bytes that will be first must be 0xaa and 0x55, either way around
		auto *start = &segment.data[0];
		if((data_size & static_cast<std::size_t>(~8191)) > 32768) {
			start = &segment.data[segment.data.size() - 16384];
		}
		if(start[0] != 0xaa && start[0] != 0x55 && start[1] != 0xaa && start[1] != 0x55) continue;
		if(start[0] == start[1]) continue;

		// probability of a random binary blob that isn't a Coleco ROM proceeding to here is 1 - 1/32768.
		if(!overflow) {
			coleco_cartridges.push_back(cartridge);
		} else {
			// Size down to a multiple of 8kb and apply the start address.
			std::vector<Storage::Cartridge::Cartridge::Segment> output_segments;

			std::vector<uint8_t> truncated_data;
			std::vector<uint8_t>::difference_type truncated_size = static_cast<std::vector<uint8_t>::difference_type>(segment.data.size()) & ~8191;
			truncated_data.insert(truncated_data.begin(), segment.data.begin(), segment.data.begin() + truncated_size);
			output_segments.emplace_back(0x8000, truncated_data);

			coleco_cartridges.emplace_back(new Storage::Cartridge::Cartridge(output_segments));
		}
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
