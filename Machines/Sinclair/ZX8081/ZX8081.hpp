//
//  ZX8081.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "Configurable/Configurable.hpp"
#include "Configurable/StandardOptions.hpp"
#include "Analyser/Static/StaticAnalyser.hpp"
#include "Machines/ROMMachine.hpp"

#include <memory>

namespace Sinclair::ZX8081 {

/// The ZX80/81 machine.
struct Machine {
	virtual ~Machine() = default;
	static std::unique_ptr<Machine> ZX8081(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);

	virtual void set_tape_is_playing(bool is_playing) = 0;
	virtual bool get_tape_is_playing() = 0;

	/// Defines the runtime options available for a ZX80/81.
	class Options: public Reflection::StructImpl<Options>, public Configurable::QuickloadOption<Options> {
		friend Configurable::QuickloadOption<Options>;
	public:
		bool automatic_tape_motor_control = true;

		Options(Configurable::OptionsType type):
			Configurable::QuickloadOption<Options>(type == Configurable::OptionsType::UserFriendly),
			automatic_tape_motor_control(type == Configurable::OptionsType::UserFriendly) {}

	private:
		Options() : Options(Configurable::OptionsType::UserFriendly) {}

		friend Reflection::StructImpl<Options>;
		void declare_fields() {
			DeclareField(automatic_tape_motor_control);
			declare_quickload_option();
		}
	};
};

}
