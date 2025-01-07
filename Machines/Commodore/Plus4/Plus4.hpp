//
//  Plus4.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/12/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../Configurable/Configurable.hpp"
#include "../../../Configurable/StandardOptions.hpp"
#include "../../../Analyser/Static/StaticAnalyser.hpp"
#include "../../ROMMachine.hpp"

#include <memory>

namespace Commodore::Plus4 {

std::unique_ptr<Reflection::Struct> get_options();

struct Machine {
	virtual ~Machine() = default;

	static std::unique_ptr<Machine> Plus4(const Analyser::Static::Target *, const ROMMachine::ROMFetcher &);

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
					Configurable::Display::SVideo : Configurable::Display::CompositeColour),
				Configurable::QuickloadOption<Options>(type == Configurable::OptionsType::UserFriendly) {
			}
	private:
		Options() : Options(Configurable::OptionsType::UserFriendly) {}

		friend Reflection::StructImpl<Options>;
		void declare_fields() {
			declare_display_option();
			declare_quickload_option();
			limit_enum(&output, Configurable::Display::SVideo, Configurable::Display::CompositeColour, -1);
		}
	};
};

}
