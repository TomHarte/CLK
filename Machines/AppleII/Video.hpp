//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef Video_hpp
#define Video_hpp

#include "../../Outputs/CRT/CRT.hpp"
#include "../../ClockReceiver/ClockReceiver.hpp"

namespace AppleII {

class Video {
	public:
		/// Constructs an instance of the video feed; a CRT is also created.
		Video();
		/// @returns The CRT this video feed is feeding.
		Outputs::CRT::CRT *get_crt();

	private:
		std::unique_ptr<Outputs::CRT::CRT> crt_;
};

}

#endif /* Video_hpp */
