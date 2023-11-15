//
//  PCCompatible.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef PCCompatible_hpp
#define PCCompatible_hpp

#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../ROMMachine.hpp"

namespace PCCompatible {

/*!
	Models a PC compatible.
*/
class Machine {
	public:
		virtual ~Machine();

		/// Creates and returns a PC Compatible.
		static Machine *PCCompatible(
			const Analyser::Static::Target *target,
			const ROMMachine::ROMFetcher &rom_fetcher
		);
};

}

#endif /* PCCompatible_hpp */
