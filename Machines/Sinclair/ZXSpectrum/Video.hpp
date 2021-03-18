//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/03/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Video_hpp
#define Video_hpp

namespace Sinclair {
namespace ZXSpectrum {

enum class VideoTiming {
	Plus3
};

template <VideoTiming timing> class Video {

};

}
}

#endif /* Video_hpp */
