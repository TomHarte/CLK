//
//  ConfidenceCounter.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "ConfidenceCounter.hpp"

using namespace Analyser::Dynamic;

float ConfidenceCounter::get_confidence() {
	return float(hits_) / float(hits_ + misses_);
}

void ConfidenceCounter::add_hit() {
	++hits_;
}

void ConfidenceCounter::add_miss() {
	++misses_;
}

void ConfidenceCounter::add_equivocal() {
	if(hits_ > misses_) {
		++hits_;
		++misses_;
	}
}
