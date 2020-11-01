//
//  Video.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/10/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "Video.hpp"

using namespace Apple::IIgs::Video;

VideoBase::VideoBase() :
	VideoSwitches<Cycles>(Cycles(2), [] (Cycles) {}) {
}

void VideoBase::did_set_annunciator_3(bool) {}
void VideoBase::did_set_alternative_character_set(bool) {}
