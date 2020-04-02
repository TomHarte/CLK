//
//  AudioProducer.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/03/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef AudioProducer_h
#define AudioProducer_h

#include "../Outputs/Speaker/Speaker.hpp"

namespace MachineTypes {

/*!
	An AudioProducer is any machine that **might** produce audio. This isn't always knowable statically.
*/
class AudioProducer {
	public:
		/// @returns The speaker that receives this machine's output, or @c nullptr if this machine is mute.
		virtual Outputs::Speaker::Speaker *get_speaker() = 0;
};

}

#endif /* AudioProducer_h */

//		virtual std::string debug_type() { return ""; }
