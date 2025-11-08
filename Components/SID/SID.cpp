//
//  SID.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/11/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include "SID.hpp"

using namespace MOS::SID;

SID::SID(Concurrency::AsyncTaskQueue<false> &audio_queue) : audio_queue_(audio_queue) {}

void SID::set_sample_volume_range(const std::int16_t range) {
	(void)range;
}

bool SID::is_zero_level() const {
	return true;
}

template <Outputs::Speaker::Action action>
void SID::apply_samples(const std::size_t number_of_samples, Outputs::Speaker::MonoSample *const target) {
	(void)number_of_samples;
	(void)target;
}

template void SID::apply_samples<Outputs::Speaker::Action::Mix>(
	std::size_t, Outputs::Speaker::MonoSample *);
template void SID::apply_samples<Outputs::Speaker::Action::Store>(
	std::size_t, Outputs::Speaker::MonoSample *);
template void SID::apply_samples<Outputs::Speaker::Action::Ignore>(
	std::size_t, Outputs::Speaker::MonoSample *);
