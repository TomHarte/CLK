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

void SID::write(const Numeric::SizedInt<5> address, const uint8_t value) {
	audio_queue_.enqueue([&] {
		switch(address.get()) {
			case 0x00:	case 0x07:	case 0x0e:	voices_[address.get() / 7].pitch.load<0>(value);		break;
			case 0x01:	case 0x08:	case 0x0f:	voices_[address.get() / 7].pitch.load<8>(value);		break;
			case 0x02:	case 0x09:	case 0x10:	voices_[address.get() / 7].pulse_width.load<0>(value);	break;
			case 0x03:	case 0x0a:	case 0x11:	voices_[address.get() / 7].pulse_width.load<8>(value);	break;
			case 0x04:	case 0x0b:	case 0x12:	voices_[address.get() / 7].control = value;				break;
			case 0x05:	case 0x0c:	case 0x13:
				voices_[address.get() / 7].attack = value >> 4;
				voices_[address.get() / 7].decay = value;
			break;
			case 0x06:	case 0x0d:	case 0x14:
				voices_[address.get() / 7].sustain = value >> 4;
				voices_[address.get() / 7].release = value;
			break;
		}
	});
}

uint8_t SID::read(const Numeric::SizedInt<5> address) {
	(void)address;
	return 0xff;
}

void SID::set_sample_volume_range(const std::int16_t range) {
	(void)range;
}

bool SID::is_zero_level() const {
	return true;
}

template <Outputs::Speaker::Action action>
void SID::apply_samples(const std::size_t number_of_samples, Outputs::Speaker::MonoSample *const target) {
	for(std::size_t c = 0; c < number_of_samples; c++) {
		voices_[0].update();
		voices_[1].update();
		voices_[2].update();
	}
	(void)number_of_samples;
	(void)target;
}

template void SID::apply_samples<Outputs::Speaker::Action::Mix>(
	std::size_t, Outputs::Speaker::MonoSample *);
template void SID::apply_samples<Outputs::Speaker::Action::Store>(
	std::size_t, Outputs::Speaker::MonoSample *);
template void SID::apply_samples<Outputs::Speaker::Action::Ignore>(
	std::size_t, Outputs::Speaker::MonoSample *);
