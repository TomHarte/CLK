//
//  ZX8081.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef ZX8081_hpp
#define ZX8081_hpp

#include "../../Reflection/Struct.h"
#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../ROMMachine.hpp"

#include <memory>

namespace ZX8081 {

/// @returns The options available for a ZX80 or ZX81.
std::unique_ptr<Reflection::Struct> get_options();

class Machine {
	public:
		virtual ~Machine();

		static Machine *ZX8081(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);

		virtual void set_tape_is_playing(bool is_playing) = 0;
		virtual bool get_tape_is_playing() = 0;
};

}

#endif /* ZX8081_hpp */
