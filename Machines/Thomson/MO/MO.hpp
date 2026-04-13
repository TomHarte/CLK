//
//  MO5.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/StaticAnalyser.hpp"
#include "Configurable/Configurable.hpp"
#include "Configurable/StandardOptions.hpp"
#include "Machines/ROMMachine.hpp"

#include <memory>

namespace Thomson::MO {

struct Machine {
	virtual ~Machine() = default;

	static std::unique_ptr<Machine> ThomsonMO(
		const Analyser::Static::Target *,
		const ROMMachine::ROMFetcher &
	);

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
		Options() : Options( Configurable::OptionsType::UserFriendly) {}

		friend Reflection::StructImpl<Options>;
		void declare_fields() {
			declare_quickload_option();
		}
	};
};

}
