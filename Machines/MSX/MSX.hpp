//
//  MSX.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "Configurable/Configurable.hpp"
#include "Configurable/StandardOptions.hpp"
#include "Analyser/Static/StaticAnalyser.hpp"
#include "Machines/ROMMachine.hpp"

#include <memory>

namespace MSX {

struct Machine {
	virtual ~Machine() = default;
	static std::unique_ptr<Machine> MSX(const Analyser::Static::Target *, const ROMMachine::ROMFetcher &);

	class Options:
		public Reflection::StructImpl<Options>,
		public Configurable::Options::Display<Options>,
		public Configurable::Options::QuickLoad<Options>
	{
		friend Configurable::Options::Display<Options>;
		friend Configurable::Options::QuickLoad<Options>;
	public:
		Options(const Configurable::OptionsType type) :
			Configurable::Options::Display<Options>(type == Configurable::OptionsType::UserFriendly ? Configurable::Display::RGB : Configurable::Display::CompositeColour),
			Configurable::Options::QuickLoad<Options>(type == Configurable::OptionsType::UserFriendly) {}

	private:
		Options() : Options( Configurable::OptionsType::UserFriendly) {}

		friend Reflection::StructImpl<Options>;
		void declare_fields() {
			declare_display_option();
			declare_quickload_option();
		}
	};
};

}
