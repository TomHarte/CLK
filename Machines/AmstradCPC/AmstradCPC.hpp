//
//  AmstradCPC.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef AmstradCPC_hpp
#define AmstradCPC_hpp

#include "../../Configurable/Configurable.hpp"
#include "../../Configurable/StandardOptions.hpp"
#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../ROMMachine.hpp"

#include <memory>

namespace AmstradCPC {

/*!
	Models an Amstrad CPC.
*/
class Machine {
	public:
		virtual ~Machine();

		/// Creates and returns an Amstrad CPC.
		static Machine *AmstradCPC(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher);

		/// Defines the runtime options available for a ZX80/81.
		class Options: public Reflection::StructImpl<Options> {
			public:
				Configurable::Display output = Configurable::Display::RGB;

				Options(Configurable::OptionsType type) {
					// Declare fields if necessary.
					if(needs_declare()) {
						DeclareField(output);
						AnnounceEnumNS(Configurable, Display);
						// TODO: some way to set limited enum support for a struct?
						// In this case, to support only RGB and CompositeColour.
					}
				}
		};
};

}

#endif /* AmstradCPC_hpp */
