//
//  MultiConfigurable.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/02/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef MultiConfigurable_hpp
#define MultiConfigurable_hpp

#include "../../../../Machines/DynamicMachine.hpp"
#include "../../../../Configurable/Configurable.hpp"

#include <memory>
#include <vector>

namespace Analyser {
namespace Dynamic {

/*!
	Provides a class that multiplexes the configurable interface to multiple machines.

	Makes a static internal copy of the list of machines; makes no guarantees about the
	order of delivered messages.
*/
class MultiConfigurable: public Configurable::Device {
	public:
		MultiConfigurable(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines);

		// Below is the standard Configurable::Device interface; see there for documentation.
		void set_options(const std::unique_ptr<Reflection::Struct> &options) final;
		std::unique_ptr<Reflection::Struct> get_options() final;

	private:
		std::vector<Configurable::Device *> devices_;
};

}
}

#endif /* MultiConfigurable_hpp */
