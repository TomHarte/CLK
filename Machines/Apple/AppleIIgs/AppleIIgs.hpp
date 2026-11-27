//
//  AppleIIgs.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/10/2020.
//  Copyright 2020 Thomas Harte. All rights reserved.
//

#pragma once

#include "Configurable/Configurable.hpp"
#include "Configurable/StandardOptions.hpp"
#include "Analyser/Static/StaticAnalyser.hpp"
#include "Machines/ROMMachine.hpp"

#include <memory>

namespace Apple::IIgs {

struct Machine {
	virtual ~Machine() = default;

	/// Factory method.
	static std::unique_ptr<Machine> create(const Analyser::Static::Target &, const ROMMachine::ROMFetcher &);

	/// Options.
	class Options:
		public Reflection::StructImpl<Options>,
		public Configurable::Options::QuickLoad<Options>
	{
		friend Configurable::Options::QuickLoad<Options>;
	public:
		Options(const Configurable::OptionsType type) :
			Configurable::Options::QuickLoad<Options>(
				type == Configurable::OptionsType::UserFriendly) {}

	private:
		Options() : Options(Configurable::OptionsType::UserFriendly) {}

		friend Reflection::StructImpl<Options>;
		void declare_fields() {
			declare_quickload_option();
		}
	};
};

}
