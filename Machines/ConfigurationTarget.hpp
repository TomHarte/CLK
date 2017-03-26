//
//  ConfigurationTarget.h
//  Clock Signal
//
//  Created by Thomas Harte on 08/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef ConfigurationTarget_hpp
#define ConfigurationTarget_hpp

#include "../StaticAnalyser/StaticAnalyser.hpp"

namespace ConfigurationTarget {

/*!
	A ConfigurationTarget::Machine is anything that can accept a StaticAnalyser::Target
	and configure itself appropriately.
*/
class Machine {
	public:
		virtual void configure_as_target(const StaticAnalyser::Target &target) = 0;
};

}

#endif /* ConfigurationTarget_h */
