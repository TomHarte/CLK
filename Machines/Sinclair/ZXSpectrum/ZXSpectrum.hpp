//
//  ZXSpectrum.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/03/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef ZXSpectrum_hpp
#define ZXSpectrum_hpp

#include "../../../Configurable/Configurable.hpp"
#include "../../../Configurable/StandardOptions.hpp"
#include "../../../Analyser/Static/StaticAnalyser.hpp"
#include "../../ROMMachine.hpp"

#include <memory>

namespace Sinclair {
namespace ZXSpectrum {

class Machine {
	public:
		virtual ~Machine();
		static Machine *ZXSpectrum(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);

		virtual void set_tape_is_playing(bool is_playing) = 0;
		virtual bool get_tape_is_playing() = 0;

		class Options: public Reflection::StructImpl<Options>, public Configurable::DisplayOption<Options>, public Configurable::QuickloadOption<Options> {
			friend Configurable::DisplayOption<Options>;
			friend Configurable::QuickloadOption<Options>;
			public:
				bool automatic_tape_motor_control = true;

				Options(Configurable::OptionsType type) :
					Configurable::DisplayOption<Options>(type == Configurable::OptionsType::UserFriendly ? Configurable::Display::RGB : Configurable::Display::CompositeColour),
					Configurable::QuickloadOption<Options>(type == Configurable::OptionsType::UserFriendly),
					automatic_tape_motor_control(type == Configurable::OptionsType::UserFriendly)
				{
					if(needs_declare()) {
						DeclareField(automatic_tape_motor_control);
						declare_display_option();
						declare_quickload_option();
					}
				}
		};
};


}
}

#endif /* ZXSpectrum_hpp */
