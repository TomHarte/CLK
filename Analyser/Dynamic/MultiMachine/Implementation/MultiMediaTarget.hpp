//
//  MultiMediaTarget.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef MultiMediaTarget_hpp
#define MultiMediaTarget_hpp

#include "../../../../Machines/MediaTarget.hpp"
#include "../../../../Machines/DynamicMachine.hpp"

#include <memory>
#include <vector>

namespace Analyser {
namespace Dynamic {

/*!
	Provides a class that multiplexes the media target interface to multiple machines.

	Makes a static internal copy of the list of machines; makes no guarantees about the
	order of delivered messages.
*/
struct MultiMediaTarget: public MediaTarget::Machine {
	public:
		MultiMediaTarget(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines);

		// Below is the standard MediaTarget::Machine interface; see there for documentation.
		bool insert_media(const Analyser::Static::Media &media) final;

	private:
		std::vector<MediaTarget::Machine *> targets_;
};

}
}

#endif /* MultiMediaTarget_hpp */
