//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/09/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include "Target.hpp"

#include <algorithm>
#include <cstring>

Analyser::Static::TargetList Analyser::Static::Sega::GetTargets(const Media &media, const std::string &file_name, TargetPlatform::IntType potential_platforms) {
	if(media.cartridges.empty())
		return {};

	TargetList targets;
	auto target = std::make_unique<Target>();

	target->machine = Machine::MasterSystem;

	// Files named .sg are treated as for the SG1000; otherwise assume a Master System.
	if(file_name.size() >= 2 && *(file_name.end() - 2) == 's' && *(file_name.end() - 1) == 'g') {
		target->model = Target::Model::SG1000;
	} else {
		target->model = Target::Model::MasterSystem;
	}

	// If this is a Master System title, look for a ROM header.
	if(target->model == Target::Model::MasterSystem) {
		const auto &data = media.cartridges.front()->get_segments()[0].data;

		// First try to locate a header.
		size_t header_offset = 0;
		size_t potential_offsets[] = {0x1ff0, 0x3ff0, 0x7ff0};
		for(auto potential_offset: potential_offsets) {
			if(!memcmp(&data[potential_offset], "TMR SEGA", 8)) {
				header_offset = potential_offset;
				break;
			}
		}

		// If a header was found, use it to crib region.
		if(header_offset) {
			// Treat export titles as European by default; decline to
			// do so only if (US) or (NTSC) is in the file name.
			const uint8_t region = data[header_offset + 0x0f] >> 4;
			switch(region) {
				default: break;
				case 4: {
					std::string lowercase_name = file_name;
					std::transform(lowercase_name.begin(), lowercase_name.end(), lowercase_name.begin(), ::tolower);
					if(lowercase_name.find("(jp)") == std::string::npos) {
						target->region =
							(lowercase_name.find("(us)") == std::string::npos &&
							lowercase_name.find("(ntsc)") == std::string::npos) ? Target::Region::Europe : Target::Region::USA;
					}
				} break;
			}

			// Also check for a Codemasters header.
			// If one is found, set the paging scheme appropriately.
			const uint16_t inverse_checksum = uint16_t(0x10000 - (data[0x7fe6] | (data[0x7fe7] << 8)));
			if(
				data[0x7fe3] >= 0x87 && data[0x7fe3] < 0x96 &&									// i.e. game is dated between 1987 and 1996
				(inverse_checksum&0xff) == data[0x7fe8] &&
				(inverse_checksum >> 8) == data[0x7fe9] &&										// i.e. the standard checksum appears to be present
				!data[0x7fea] && !data[0x7feb] && !data[0x7fec] && !data[0x7fed] && !data[0x7fee] && !data[0x7fef]
			) {
				target->paging_scheme = Target::PagingScheme::Codemasters;
				target->model = Target::Model::MasterSystem2;
			}
		}
	}

	target->media.cartridges = media.cartridges;
	targets.push_back(std::move(target));
	return targets;
}
