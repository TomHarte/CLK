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
		Bus &bus_;
		const size_t device_id_;

		std::vector<uint8_t> response_;
		int bit_offset_ = 0;
		double microseconds_at_bit_ = 0;
};

}
}

#endif /* ReactiveDevice_hpp */
