//
//  Archimedes.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "Configurable/Configurable.hpp"
#include "Configurable/StandardOptions.hpp"
#include "Analyser/Static/StaticAnalyser.hpp"
#include "Machines/ROMMachine.hpp"

#include <memory>

namespace Archimedes {

struct Machine {
	virtual ~Machine() = default;
	static std::unique_ptr<Machine> Archimedes(
		const Analyser::Static::Target *target,
		const ROMMachine::ROMFetcher &rom_fetcher
	);

	class Options: public Reflection::StructImpl<Options>, public Configurable::Options::QuickLoad<Options> {
		friend Configurable::Options::QuickLoad<Options>;
	public:
		Options(const Configurable::OptionsType type) :
			Configurable::Options::QuickLoad<Options>(type == Configurable::OptionsType::UserFriendly) {}
	private:
		Options() : Options( Configurable::OptionsType::UserFriendly) {}
		friend Reflection::StructImpl<Options>;
		void declare_fields() {
			declare_quickload_option();
		}
	};
};

}
