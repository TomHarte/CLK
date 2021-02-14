//
//  Keyboard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 13/02/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Keyboard_hpp
#define Keyboard_hpp

#include "ReactiveDevice.hpp"

namespace Apple {
namespace ADB {

class Keyboard: public ReactiveDevice {
	public:
		Keyboard(Bus &);

	private:
		void perform_command(const Command &command) override;
};

}
}

#endif /* Keyboard_hpp */
