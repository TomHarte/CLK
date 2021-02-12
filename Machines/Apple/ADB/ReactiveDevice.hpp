//
//  ReactiveDevice.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/02/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef ReactiveDevice_hpp
#define ReactiveDevice_hpp

#include "Bus.hpp"

#include <cstddef>
#include <vector>

namespace Apple {
namespace ADB {

class ReactiveDevice: public Bus::Device {
	protected:
		ReactiveDevice(Bus &bus);

		void post_response(const std::vector<uint8_t> &&response);
		void advance_state(double microseconds) override;

	private:
		const size_t device_id_;
		std::vector<uint8_t> response_;
};

}
}

#endif /* ReactiveDevice_hpp */
