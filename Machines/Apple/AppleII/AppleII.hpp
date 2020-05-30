//
//  AppleII.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef AppleII_hpp
#define AppleII_hpp

#include "../../../Configurable/Configurable.hpp"
#include "../../../Configurable/StandardOptions.hpp"
#include "../../../Analyser/Static/StaticAnalyser.hpp"
#include "../../ROMMachine.hpp"

#include <memory>

namespace Apple {
namespace II {

class Machine {
	public:
		virtual ~Machine();

		/// Creates and returns an AppleII.
		static Machine *AppleII(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);

		/// Defines the runtime options available for an Apple II.
		class Options: public Reflection::StructImpl<Options>, public Configurable::DisplayOption<Options> {
			friend Configurable::DisplayOption<Options>;
			public:
				Options(Configurable::OptionsType) : Configurable::DisplayOption<Options>(Configurable::Display::CompositeColour)  {
					if(needs_declare()) {
						declare_display_option();
						limit_enum(&output, Configurable::Display::CompositeMonochrome, Configurable::Display::CompositeColour, -1);
					}
				}
		};
};

}
}

#endif /* AppleII_hpp */
