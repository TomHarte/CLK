//
//  ConfigurationTarget.h
//  Clock Signal
//
//  Created by Thomas Harte on 08/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef ConfigurationTarget_hpp
#define ConfigurationTarget_hpp

#include "../Analyser/Static/StaticAnalyser.hpp"
#include "../Configurable/Configurable.hpp"

#include <string>

namespace ConfigurationTarget {

/*!
	A ConfigurationTarget::Machine is anything that can accept a Analyser::Static::Target
	and configure itself appropriately, or accept a list of media subsequently to insert.
*/
class Machine {
	public:
		/// Instructs the machine to configure itself as described by @c target and insert the included media.
		virtual void configure_as_target(const Analyser::Static::Target *target) = 0;

		/*!
			Requests that the machine insert @c media as a modification to current state

			@returns @c true if any media was inserted; @c false otherwise.
		*/
		virtual bool insert_media(const Analyser::Static::Media &media) = 0;
};

}

#endif /* ConfigurationTarget_h */
