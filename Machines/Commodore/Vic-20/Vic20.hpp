//
//  Vic20.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../Configurable/Configurable.hpp"
#include "../../../Configurable/StandardOptions.hpp"
#include "../../../Analyser/Static/StaticAnalyser.hpp"
#include "../../ROMMachine.hpp"

#include <memory>

namespace Commodore::Vic20 {

/// @returns The options available for a Vic-20.
std::unique_ptr<Reflection::Struct> get_options();

class Machine {
public:
	virtual ~Machine() = default;

	/// Creates and returns a Vic-20.
	static std::unique_ptr<Machine> Vic20(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);

	class Options: public Reflection::StructImpl<Options>, public Configurable::DisplayOption<Options>, public Configurable::QuickloadOption<Options> {
		friend Configurable::DisplayOption<Options>;
		friend Configurable::QuickloadOption<Options>;
	public:
		Options(Configurable::OptionsType type) :
			Configurable::DisplayOption<Options>(type == Configurable::OptionsType::UserFriendly ? Configurable::Display::SVideo : Configurable::Display::CompositeColour),
			Configurable::QuickloadOption<Options>(type == Configurable::OptionsType::UserFriendly) {
			if(needs_declare()) {
				declare_display_option();
				declare_quickload_option();
				limit_enum(&output, Configurable::Display::SVideo, Configurable::Display::CompositeColour, -1);
			}
		}
	};
};

}
