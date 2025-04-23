//
//  Enterprise.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/StaticAnalyser.hpp"
#include "Configurable/Configurable.hpp"
#include "Configurable/StandardOptions.hpp"
#include "Machines/ROMMachine.hpp"

#include <memory>

namespace Enterprise {

/*!
	@abstract Represents an Elan Enterprise.

	@discussion An instance of Enterprise::Machine represents the current state of an
	Elan Enterprise.
*/
struct Machine {
	virtual ~Machine() = default;
	static std::unique_ptr<Machine> Enterprise(const Analyser::Static::Target *, const ROMMachine::ROMFetcher &);

	/// Defines the runtime options available for an Enterprise.
	class Options: public Reflection::StructImpl<Options>, public Configurable::DisplayOption<Options> {
		friend Configurable::DisplayOption<Options>;
	public:
		Options(Configurable::OptionsType type) :
			Configurable::DisplayOption<Options>(
				type == Configurable::OptionsType::UserFriendly ?
					Configurable::Display::RGB : Configurable::Display::CompositeColour) {}

	private:
		Options() : Options(Configurable::OptionsType::UserFriendly) {}

		friend Reflection::StructImpl<Options>;
		void declare_fields() {
			declare_display_option();
			limit_enum(&output,
				Configurable::Display::RGB, Configurable::Display::CompositeColour,
				Configurable::Display::CompositeMonochrome, -1);
		}
	};
};

};
