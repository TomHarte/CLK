//
//  SWIIndex.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/05/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#ifndef SWIIndex_hpp
#define SWIIndex_hpp

#include <array>
#include <cstdint>
#include <string>

namespace Analyser::Static::Acorn {

enum class SWIGroup: uint8_t {
	OperatingSystem = 0b00,
	OperatingSystemModules = 0b01,
	ThirdPartyApplications = 0b10,
	UserApplications = 0b11,
};

struct SWIDescription {
	SWIDescription(uint32_t comment);

	uint8_t chunk_offset;
	SWIGroup swi_group;
	uint16_t chunk_number;
	uint8_t os_flag;
	bool error_flag;

	std::string name;
	struct Register {
		enum class Type {
			Unused,
			ReasonCode,
			Pointer,
			PointerToString,
			ReasonCodeDependent,
			Character,

			/// A string that appears immediately after the SWI in memory.
			FollowingString,
		} type = Type::Unused;
	};
	std::array<Register, 14> registers;
};

}

#endif /* SWIIndex_hpp */
