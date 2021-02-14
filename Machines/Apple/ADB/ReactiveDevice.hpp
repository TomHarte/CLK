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
		ReactiveDevice(Bus &bus, uint8_t adb_device_id);

		void post_response(const std::vector<uint8_t> &&response);
		void post_service_request();
		virtual void perform_command(const Command &command) = 0;

	private:
		void advance_state(double microseconds, bool current_level) override;
		void adb_bus_did_observe_event(Bus::Event event, uint8_t value) override;

	private:
		Bus &bus_;
		const size_t device_id_;

		std::vector<uint8_t> response_;
		int bit_offset_ = 0;
		double microseconds_at_bit_ = 0;

		bool next_is_command_ = false;
		uint8_t adb_device_id_;
};

}
}

#endif /* ReactiveDevice_hpp */
