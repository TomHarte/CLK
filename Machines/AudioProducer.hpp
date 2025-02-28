//
//  AudioProducer.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/03/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#pragma once

#include "Outputs/Speaker/Speaker.hpp"

namespace MachineTypes {

/*!
	An AudioProducer is any machine that **might** produce audio. This isn't always knowable statically.
*/
struct AudioProducer {
	/// @returns The speaker that receives this machine's output, or @c nullptr if this machine is mute.
	virtual Outputs::Speaker::Speaker *get_speaker() = 0;
};

}
