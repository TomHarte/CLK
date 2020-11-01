//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/10/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef Apple_IIgs_Video_hpp
#define Apple_IIgs_Video_hpp

#include "../AppleII/VideoSwitches.hpp"

namespace Apple {
namespace IIgs {
namespace Video {

class VideoBase: public Apple::II::VideoSwitches<Cycles> {
	public:
		VideoBase();

	private:
		void did_set_annunciator_3(bool) override;
		void did_set_alternative_character_set(bool) override;
};

class Video: public VideoBase {
	public:
		using VideoBase::VideoBase;
};

}
}
}

#endif /* Video_hpp */
