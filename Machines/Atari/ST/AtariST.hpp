//
//  AtariST.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef AtariST_hpp
#define AtariST_hpp

#include "../../../Configurable/Configurable.hpp"
#include "../../../Configurable/StandardOptions.hpp"
#include "../../../Analyser/Static/StaticAnalyser.hpp"
#include "../../ROMMachine.hpp"

#include <memory>

namespace Atari {
namespace ST {

class Machine {
	public:
		virtual ~Machine();

		static Machine *AtariST(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);

		class Options: public Reflection::StructImpl<Options>, public Configurable::DisplayOption<Options> {
			friend Configurable::DisplayOption<Options>;
			public:
				Options(Configurable::OptionsType type) : Configurable::DisplayOption<Options>(
					type == Configurable::OptionsType::UserFriendly ? Configurable::Display::RGB : Configurable::Display::CompositeColour)  {
					if(needs_declare()) {
						declare_display_option();
						limit_enum(&output, Configurable::Display::RGB, Configurable::Display::CompositeColour, -1);
					}
				}
		};
};

}
}
#endif /* AtariST_hpp */
