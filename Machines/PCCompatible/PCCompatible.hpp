//
//  PCCompatible.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../Configurable/Configurable.hpp"
#include "../../Configurable/StandardOptions.hpp"
#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../ROMMachine.hpp"

namespace PCCompatible {

/*!
	Models a PC compatible.
*/
class Machine {
	public:
		virtual ~Machine();

		/// Creates and returns a PC Compatible.
		static std::unique_ptr<Machine> PCCompatible(
			const Analyser::Static::Target *target,
			const ROMMachine::ROMFetcher &rom_fetcher
		);

		/// Defines the runtime options [sometimes] available for a PC.
		class Options:
			public Reflection::StructImpl<Options>,
			public Configurable::DisplayOption<Options>
		{
			friend Configurable::DisplayOption<Options>;
			public:
				Options(Configurable::OptionsType) :
					Configurable::DisplayOption<Options>(Configurable::Display::RGB)
				{
					if(needs_declare()) {
						declare_display_option();
						limit_enum(&output, Configurable::Display::RGB, Configurable::Display::CompositeColour, -1);
					}
				}
		};
};

}
