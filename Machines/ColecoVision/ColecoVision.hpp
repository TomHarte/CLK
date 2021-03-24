//
//  ColecoVision.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/02/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef ColecoVision_hpp
#define ColecoVision_hpp

#include "../../Configurable/Configurable.hpp"
#include "../../Configurable/StandardOptions.hpp"
#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../ROMMachine.hpp"

namespace Coleco {
namespace Vision {

class Machine {
	public:
		virtual ~Machine();
		static Machine *ColecoVision(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);

		class Options: public Reflection::StructImpl<Options>, public Configurable::DisplayOption<Options> {
			friend Configurable::DisplayOption<Options>;
			public:
				Options(Configurable::OptionsType type) :
					Configurable::DisplayOption<Options>(type == Configurable::OptionsType::UserFriendly ? Configurable::Display::SVideo : Configurable::Display::CompositeColour) {
					if(needs_declare()) {
						declare_display_option();
						limit_enum(&output, Configurable::Display::SVideo, Configurable::Display::CompositeColour, -1);
					}
				}
		};
};

}
}

#endif /* ColecoVision_hpp */
