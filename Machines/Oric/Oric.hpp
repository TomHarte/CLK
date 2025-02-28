//
//  Oric.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/10/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "Configurable/Configurable.hpp"
#include "Configurable/StandardOptions.hpp"
#include "Analyser/Static/StaticAnalyser.hpp"
#include "Machines/ROMMachine.hpp"

#include <memory>

namespace Oric {

/*!
	Models an Oric 1/Atmos with or without a Microdisc.
*/
struct Machine {
	virtual ~Machine() = default;
	static std::unique_ptr<Machine> Oric(const Analyser::Static::Target *, const ROMMachine::ROMFetcher &);

	class Options:
		public Reflection::StructImpl<Options>,
		public Configurable::DisplayOption<Options>,
		public Configurable::QuickloadOption<Options>
	{
		friend Configurable::DisplayOption<Options>;
		friend Configurable::QuickloadOption<Options>;
	public:
		Options(Configurable::OptionsType type) :
			Configurable::DisplayOption<Options>(type == Configurable::OptionsType::UserFriendly ?
				Configurable::Display::RGB : Configurable::Display::CompositeColour),
			Configurable::QuickloadOption<Options>(type == Configurable::OptionsType::UserFriendly) {}

	private:
		Options() : Options(Configurable::OptionsType::UserFriendly) {}

		friend Reflection::StructImpl<Options>;
		void declare_fields() {
			declare_display_option();
			declare_quickload_option();
		}
	};
};

}
