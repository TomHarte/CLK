//
//  KonamiSCC.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/01/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "KonamiSCC.hpp"

#include <cstring>

using namespace Konami;

SCC::SCC(Concurrency::DeferringAsyncTaskQueue &task_queue) :
	task_queue_(task_queue) {}

bool SCC::is_silent() {
	return !(channel_enable_ & 0x1f);
}

void SCC::get_samples(std::size_t number_of_samples, std::int16_t *target) {
	if(is_silent()) {
		std::memset(target, 0, sizeof(std::int16_t) * number_of_samples);
		return;
	}

	// TODO
}

void SCC::write(uint16_t address, uint8_t value) {
	// TODO
}

uint8_t SCC::read(uint16_t address) {
	// TODO
	return 0xff;
}
