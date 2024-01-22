//
//  MasterSystem.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/09/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../Configurable/Configurable.hpp"
#include "../../Configurable/StandardOptions.hpp"
#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../ROMMachine.hpp"

#include <memory>

namespace Sega::MasterSystem {

class Machine {
	public:
		virtual ~Machine();
		static std::unique_ptr<Machine> MasterSystem(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);

		class Options: public Reflection::StructImpl<Options>, public Configurable::DisplayOption<Options> {
			friend Configurable::DisplayOption<Options>;
			public:
				Options(Configurable::OptionsType type) :
					Configurable::DisplayOption<Options>(type == Configurable::OptionsType::UserFriendly ? Configurable::Display::RGB : Configurable::Display::CompositeColour) {
					if(needs_declare()) {
						declare_display_option();
					}
				}
		};
};

}
