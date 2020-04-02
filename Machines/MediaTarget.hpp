//
//  MediaTarget.h
//  Clock Signal
//
//  Created by Thomas Harte on 08/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef MediaTarget_hpp
#define MediaTarget_hpp

#include "../Analyser/Static/StaticAnalyser.hpp"
#include "../Configurable/Configurable.hpp"

#include <string>

namespace MachineTypes {

/*!
	A MediaTarget::Machine is anything that can accept new media while running.
*/
class MediaTarget {
	public:
		/*!
			Requests that the machine insert @c media as a modification to current state

			@returns @c true if any media was inserted; @c false otherwise.
		*/
		virtual bool insert_media(const Analyser::Static::Media &media) = 0;
};

}

#endif /* MediaTarget_hpp */
