//
//  Macintosh.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/05/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../Configurable/Configurable.hpp"
#include "../../../Configurable/StandardOptions.hpp"
#include "../../../Analyser/Static/StaticAnalyser.hpp"
#include "../../ROMMachine.hpp"

namespace Apple::Macintosh {

struct Machine {
	virtual ~Machine() = default;

	/// Creates and returns a Macintosh.
	static std::unique_ptr<Machine> Macintosh(const Analyser::Static::Target *, const ROMMachine::ROMFetcher &);

	class Options: public Reflection::StructImpl<Options>, public Configurable::QuickbootOption<Options> {
		friend Configurable::QuickbootOption<Options>;
	public:
		Options(Configurable::OptionsType type) :
			Configurable::QuickbootOption<Options>(type == Configurable::OptionsType::UserFriendly) {}
	private:
		Options() : Options(Configurable::OptionsType::UserFriendly) {}

		friend Reflection::StructImpl<Options>;
		void declare_fields() {
			declare_quickboot_option();
		}
	};
};

}
