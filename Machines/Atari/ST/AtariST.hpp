//
//  AtariST.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#pragma once

#include "Configurable/Configurable.hpp"
#include "Configurable/StandardOptions.hpp"
#include "Analyser/Static/StaticAnalyser.hpp"
#include "Machines/ROMMachine.hpp"

#include <memory>

namespace Atari::ST {

struct Machine {
	virtual ~Machine() = default;

	static std::unique_ptr<Machine> AtariST(const Analyser::Static::Target *, const ROMMachine::ROMFetcher &);

	class Options: public Reflection::StructImpl<Options>, public Configurable::DisplayOption<Options> {
		friend Configurable::DisplayOption<Options>;
	public:
		Options(Configurable::OptionsType type) : Configurable::DisplayOption<Options>(
			type == Configurable::OptionsType::UserFriendly ?
				Configurable::Display::RGB : Configurable::Display::CompositeColour) {}

	private:
		Options() : Options(Configurable::OptionsType::UserFriendly) {}

		friend Reflection::StructImpl<Options>;
		void declare_fields() {
			declare_display_option();
			limit_enum(&output, Configurable::Display::RGB, Configurable::Display::CompositeColour, -1);
		}
	};
};

}
