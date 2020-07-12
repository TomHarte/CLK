//
//  UnformattedTrack.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/08/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "UnformattedTrack.hpp"

using namespace Storage::Disk;

Track::Event UnformattedTrack::get_next_event() {
	Track::Event event;
	event.type = Event::IndexHole;
	event.length = Time(1);
	return event;
}

Storage::Time UnformattedTrack::seek_to(const Time &) {
	return Time(0);
}

Track *UnformattedTrack::clone() const {
	return new UnformattedTrack;
}
