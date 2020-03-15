//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/02/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

static std::vector<std::shared_ptr<Storage::Cartridge::Cartridge>>
		ColecoCartridgesFrom(const std::vector<std::shared_ptr<Storage::Cartridge::Cartridge>> &cartridges) {
	std::vector<std::shared_ptr<Storage::Cartridge::Cartridge>> coleco_cartridges;

	for(const auto &cartridge : cartridges) {
		const auto &segments = cartridge->get_segments();

		// only one mapped item is allowed
		if(segments.size() != 1) continue;
		const Storage::Cartridge::Cartridge::Segment &segment = segments.front();
		const std::size_t data_size = segment.data.size();

		// the two bytes that will be first must be 0xaa and 0x55, either way around
		auto *start = &segment.data[0];
		if((data_size & static_cast<std::size_t>(~8191)) > 32768) {
			start = &segment.data[segment.data.size() - 16384];
		}
		if(start[0] != 0xaa && start[0] != 0x55 && start[1] != 0xaa && start[1] != 0x55) continue;
		if(start[0] == start[1]) continue;

		// probability of a random binary blob that isn't a Coleco ROM proceeding to here is 1 - 1/32768.

		// Round up to the next multiple of 8kb if this image is less than 32kb. Otherwise round down if
		// this image is within a short distance of 32kb.
		std::vector<Storage::Cartridge::Cartridge::Segment> output_segments;

		size_t target_size;
		if(data_size >= 32*1024 && data_size < 32*1024 + 512) {
			target_size = 32 * 1024;
		} else {
			target_size = data_size + ((8192 - (data_size & 8191)) & 8191);
		}

		std::vector<uint8_t> truncated_data;
		truncated_data = segment.data;
		truncated_data.resize(target_size);
		output_segments.emplace_back(0x8000, truncated_data);

		coleco_cartridges.emplace_back(new Storage::Cartridge::Cartridge(output_segments));
	}

	return coleco_cartridges;
}

Analyser::Static::TargetList Analyser::Static::Coleco::GetTargets(const Media &media, const std::string &file_name, TargetPlatform::IntType potential_platforms) {
	TargetList targets;
	auto target = std::make_unique<Target>(Machine::ColecoVision);
	target->confidence = 1.0f - 1.0f / 32768.0f;
	target->media.cartridges = ColecoCartridgesFrom(media.cartridges);
	if(!target->media.empty())
		targets.push_back(std::move(target));
	return targets;
}
