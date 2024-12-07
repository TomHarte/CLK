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

struct Machine {
	virtual ~Machine() = default;
	static std::unique_ptr<Machine> MasterSystem(const Analyser::Static::Target *, const ROMMachine::ROMFetcher &);

	class Options: public Reflection::StructImpl<Options>, public Configurable::DisplayOption<Options> {
		friend Configurable::DisplayOption<Options>;
		public:
			Options(Configurable::OptionsType type) :
				Configurable::DisplayOption<Options>(type == Configurable::OptionsType::UserFriendly ?
					Configurable::Display::RGB : Configurable::Display::CompositeColour) {}

	private:
		Options() : Options(Configurable::OptionsType::UserFriendly) {}

		friend Reflection::StructImpl<Options>;
		void declare_fields() {
			declare_display_option();
		}
	};
};

}
