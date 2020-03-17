//
//  ZX8081.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef ZX8081_hpp
#define ZX8081_hpp

#include "../../Configurable/Configurable.hpp"
#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../ROMMachine.hpp"

#include <memory>

namespace ZX8081 {

/// The ZX80/81 machine.
class Machine {
	public:
		virtual ~Machine();

		static Machine *ZX8081(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);

		virtual void set_tape_is_playing(bool is_playing) = 0;
		virtual bool get_tape_is_playing() = 0;

		/// Defines the runtime options available for a ZX80/81.
		class Options: public Reflection::StructImpl<Options> {
			public:
				bool automatic_tape_motor_control;
				bool quickload;

				Options(Configurable::OptionsType type):
					automatic_tape_motor_control(type == Configurable::OptionsType::UserFriendly),
					quickload(automatic_tape_motor_control) {

					// Declare fields if necessary.
					if(needs_declare()) {
						DeclareField(automatic_tape_motor_control);
						DeclareField(quickload);
					}
				}
		};
};

}

#endif /* ZX8081_hpp */
