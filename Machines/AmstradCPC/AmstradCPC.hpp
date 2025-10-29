//
//  AmstradCPC.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "Configurable/Configurable.hpp"
#include "Configurable/StandardOptions.hpp"
#include "Analyser/Static/StaticAnalyser.hpp"
#include "Machines/ROMMachine.hpp"

#include <memory>

namespace AmstradCPC {

/*!
	Models an Amstrad CPC.
*/
struct Machine {
	virtual ~Machine() = default;

	/// Creates and returns an Amstrad CPC.
	static std::unique_ptr<Machine> AmstradCPC(
		const Analyser::Static::Target *const target,
		const ROMMachine::ROMFetcher &rom_fetcher
	);

	/// Defines the runtime options available for an Amstrad CPC.
	class Options:
		public Reflection::StructImpl<Options>,
		public Configurable::Options::Display<Options>,
		public Configurable::Options::QuickLoad<Options>
	{
	public:
		Options(const Configurable::OptionsType type) :
			Configurable::Options::Display<Options>(Configurable::Display::RGB),
			Configurable::Options::QuickLoad<Options>(type == Configurable::OptionsType::UserFriendly) {}

	private:
		friend Configurable::Options::Display<Options>;
		friend Configurable::Options::QuickLoad<Options>;

		Options() : Options( Configurable::OptionsType::UserFriendly) {}

		friend Reflection::StructImpl<Options>;
		void declare_fields() {
			declare_display_option();
			declare_quickload_option();
			limit_enum(&output, Configurable::Display::RGB, Configurable::Display::CompositeColour, -1);
		}
	};

	struct SSMDelegate {
		virtual void perform(uint16_t) = 0;
	};
	virtual void set_ssm_delegate(SSMDelegate *) = 0;
};

}
