//
//  AmstradCPC.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../Configurable/Configurable.hpp"
#include "../../Configurable/StandardOptions.hpp"
#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../ROMMachine.hpp"

#include <memory>

namespace AmstradCPC {

/*!
	Models an Amstrad CPC.
*/
class Machine {
	public:
		virtual ~Machine();

		/// Creates and returns an Amstrad CPC.
		static std::unique_ptr<Machine> AmstradCPC(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);

		/// Defines the runtime options available for an Amstrad CPC.
		class Options:
			public Reflection::StructImpl<Options>,
			public Configurable::DisplayOption<Options>,
			public Configurable::QuickloadOption<Options>
		{
			friend Configurable::DisplayOption<Options>;
			friend Configurable::QuickloadOption<Options>;
			public:
				Options(Configurable::OptionsType type) :
					Configurable::DisplayOption<Options>(Configurable::Display::RGB),
					Configurable::QuickloadOption<Options>(type == Configurable::OptionsType::UserFriendly)
				{
					if(needs_declare()) {
						declare_display_option();
						declare_quickload_option();
						limit_enum(&output, Configurable::Display::RGB, Configurable::Display::CompositeColour, -1);
					}
				}
		};
};

}
