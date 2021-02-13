//
//  Mouse.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/02/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Mouse_hpp
#define Mouse_hpp

#include "ReactiveDevice.hpp"

namespace Apple {
namespace ADB {

class Mouse: public ReactiveDevice {
	public:
		Mouse(Bus &);

		void adb_bus_did_observe_event(Bus::Event event, uint8_t value) override;

	private:
		bool next_is_command_ = false;
};

}
}

#endif /* Mouse_hpp */
