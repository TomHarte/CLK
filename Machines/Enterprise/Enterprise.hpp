//
//  Enterprise.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Enterprise_hpp
#define Enterprise_hpp

#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../../Configurable/Configurable.hpp"
#include "../../Configurable/StandardOptions.hpp"
#include "../ROMMachine.hpp"

namespace Enterprise {

/*!
	@abstract Represents an Elan Enterprise.

	@discussion An instance of Enterprise::Machine represents the current state of an
	Elan Enterprise.
*/
class Machine {
	public:
		virtual ~Machine();

		static Machine *Enterprise(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);

		/// Defines the runtime options available for an Enterprise.
		class Options: public Reflection::StructImpl<Options>, public Configurable::DisplayOption<Options> {
			friend Configurable::DisplayOption<Options>;
			public:
				Options(Configurable::OptionsType type) :
					Configurable::DisplayOption<Options>(type == Configurable::OptionsType::UserFriendly ? Configurable::Display::RGB : Configurable::Display::CompositeColour) {
					if(needs_declare()) {
						declare_display_option();
						limit_enum(&output, Configurable::Display::RGB, Configurable::Display::CompositeColour, Configurable::Display::CompositeMonochrome, -1);
					}
				}
		};
};

};

#endif /* Enterprise_hpp */
