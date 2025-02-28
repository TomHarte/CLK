//
//  AppleII.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "Configurable/Configurable.hpp"
#include "Configurable/StandardOptions.hpp"
#include "Analyser/Static/StaticAnalyser.hpp"
#include "Machines/ROMMachine.hpp"

#include <memory>

namespace Apple::II {

struct Machine {
	virtual ~Machine() = default;

	/// Creates and returns an AppleII.
	static std::unique_ptr<Machine> AppleII(const Analyser::Static::Target *, const ROMMachine::ROMFetcher &);

	/// Defines the runtime options available for an Apple II.
	class Options: public Reflection::StructImpl<Options>, public Configurable::DisplayOption<Options> {
		friend Configurable::DisplayOption<Options>;
	public:
		bool use_square_pixels = false;

		Options(Configurable::OptionsType) :
			Configurable::DisplayOption<Options>(Configurable::Display::CompositeColour) {}
	private:
		Options() : Options(Configurable::OptionsType::UserFriendly) {}

		friend Reflection::StructImpl<Options>;
		void declare_fields() {
			DeclareField(use_square_pixels);
			declare_display_option();
			limit_enum(&output, Configurable::Display::CompositeMonochrome, Configurable::Display::CompositeColour, -1);
		}
	};
};

}
