//
//  Electron.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/01/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/StaticAnalyser.hpp"
#include "Configurable/Configurable.hpp"
#include "Configurable/StandardOptions.hpp"
#include "Machines/ROMMachine.hpp"

#include <memory>

namespace Electron {

/*!
	@abstract Represents an Acorn Electron.

	@discussion An instance of Electron::Machine represents the current state of an
	Acorn Electron.
*/
struct Machine {
	virtual ~Machine() = default;

	/// Creates and returns an Electron.
	static std::unique_ptr<Machine> Electron(
		const Analyser::Static::Target *target,
		const ROMMachine::ROMFetcher &rom_fetcher
	);

	/// Defines the runtime options available for an Electron.
	class Options:
		public Reflection::StructImpl<Options>,
		public Configurable::DisplayOption<Options>,
		public Configurable::QuickloadOption<Options>
	{
		friend Configurable::DisplayOption<Options>;
		friend Configurable::QuickloadOption<Options>;
	public:
		Options(const Configurable::OptionsType type) :
			Configurable::DisplayOption<Options>(
				type == Configurable::OptionsType::UserFriendly ?
					Configurable::Display::RGB : Configurable::Display::CompositeColour
			),
			Configurable::QuickloadOption<Options>(
				type == Configurable::OptionsType::UserFriendly) {}

	private:
		Options() : Options(Configurable::OptionsType::UserFriendly) {}

		friend Reflection::StructImpl<Options>;
		void declare_fields() {
			declare_display_option();
			declare_quickload_option();
			limit_enum(
				&output,
				Configurable::Display::RGB,
				Configurable::Display::CompositeColour,
				Configurable::Display::CompositeMonochrome,
				-1
			);
		}
	};
};

}
