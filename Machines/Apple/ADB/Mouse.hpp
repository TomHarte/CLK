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

		void perform_command(const Command &command) override;
};

}
}

#endif /* Mouse_hpp */
