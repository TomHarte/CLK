//
//  MultiConfigurationTarget.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/01/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef MultiConfigurationTarget_hpp
#define MultiConfigurationTarget_hpp

#include "../../../../Machines/ConfigurationTarget.hpp"
#include "../../../../Machines/DynamicMachine.hpp"

#include <memory>
#include <vector>

namespace Analyser {
namespace Dynamic {

struct MultiConfigurationTarget: public ::ConfigurationTarget::Machine {
	public:
		MultiConfigurationTarget(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines);

		void configure_as_target(const Analyser::Static::Target &target) override;
		bool insert_media(const Analyser::Static::Media &media) override;

	private:
		const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines_;
};

}
}

#endif /* MultiConfigurationTarget_hpp */
