//
//  ZXSpectrum.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/03/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include "Configurable/Configurable.hpp"
#include "Configurable/StandardOptions.hpp"
#include "Analyser/Static/StaticAnalyser.hpp"
#include "Machines/ROMMachine.hpp"

#include <memory>

namespace Sinclair::ZXSpectrum {

struct Machine {
	virtual ~Machine() = default;
	static std::unique_ptr<Machine> ZXSpectrum(const Analyser::Static::Target *, const ROMMachine::ROMFetcher &);

	virtual void set_tape_is_playing(bool is_playing) = 0;
	virtual bool get_tape_is_playing() = 0;

	class Options:
		public Reflection::StructImpl<Options>,
		public Configurable::Options::Display<Options>,
		public Configurable::Options::QuickLoad<Options>
	{
		friend Configurable::Options::Display<Options>;
		friend Configurable::Options::QuickLoad<Options>;
	public:
		bool automatic_tape_motor_control = true;

		Options(const Configurable::OptionsType type) :
			Configurable::Options::Display<Options>(type == Configurable::OptionsType::UserFriendly ? Configurable::Display::RGB : Configurable::Display::CompositeColour),
			Configurable::Options::QuickLoad<Options>(type == Configurable::OptionsType::UserFriendly),
			automatic_tape_motor_control(type == Configurable::OptionsType::UserFriendly) {}

	private:
		Options() : Options( Configurable::OptionsType::UserFriendly) {}

		friend Reflection::StructImpl<Options>;
		void declare_fields() {
			DeclareField(automatic_tape_motor_control);
			declare_display_option();
			declare_quickload_option();
			limit_enum(&output, Configurable::Display::RGB, Configurable::Display::CompositeColour, -1);
		}
	};
};

}
