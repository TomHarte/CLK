//
//  MultiConfigurable.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/02/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef MultiConfigurable_hpp
#define MultiConfigurable_hpp

#include "../../../../Machines/DynamicMachine.hpp"

#include <memory>
#include <vector>

namespace Analyser {
namespace Dynamic {

class MultiConfigurable: public Configurable::Device {
	public:
		MultiConfigurable(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines);

		std::vector<std::unique_ptr<Configurable::Option>> get_options() override;
		void set_selections(const Configurable::SelectionSet &selection_by_option) override;
		Configurable::SelectionSet get_accurate_selections() override;
		Configurable::SelectionSet get_user_friendly_selections() override;

	private:
		std::vector<Configurable::Device *> devices_;
};

}
}

#endif /* MultiConfigurable_hpp */
