//
//  BBCMicro.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/09/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/StaticAnalyser.hpp"
#include "Configurable/Configurable.hpp"
#include "Configurable/StandardOptions.hpp"
#include "Machines/ROMMachine.hpp"

#include <memory>

namespace BBCMicro {

struct Machine {
	virtual ~Machine() = default;

	static std::unique_ptr<Machine> BBCMicro(
		const Analyser::Static::Target *target,
		const ROMMachine::ROMFetcher &rom_fetcher
	);

	class Options:
		public Reflection::StructImpl<Options>,
		public Configurable::Options::DynamicCrop<Options>
	{
	public:
		Options(const Configurable::OptionsType type) :
			Configurable::Options::DynamicCrop<Options>(type == Configurable::OptionsType::UserFriendly) {}

	private:
		friend Configurable::Options::DynamicCrop<Options>;

		Options() : Options( Configurable::OptionsType::UserFriendly) {}

		friend Reflection::StructImpl<Options>;
		void declare_fields() {
			declare_dynamic_crop_option();
		}
	};
};

}

