//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

void StaticAnalyser::MSX::AddTargets(const Media &media, std::list<Target> &destination) {
	// Very trusting...
	Target target;
	target.machine = Target::MSX;
	target.probability = 1.0;
	target.media = media;
	destination.push_back(target);
}
