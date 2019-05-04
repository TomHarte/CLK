//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/05/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef Video_hpp
#define Video_hpp

#include "../../../Outputs/CRT/CRT.hpp"

namespace Apple {
namespace Macintosh {

class Video {
	public:
		Video(uint16_t *ram);
		void set_scan_target(Outputs::Display::ScanTarget *scan_target);

	private:
		Outputs::CRT::CRT crt_;
};

}
}

#endif /* Video_hpp */
