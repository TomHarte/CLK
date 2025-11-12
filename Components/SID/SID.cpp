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
	last_write_ = value;
	audio_queue_.enqueue([=, this] {
		const auto voice = [&]() -> Voice & {
			return voices_[address.get() / 7];
		};
		const auto oscillator = [&]() -> Voice::Oscillator & {
			return voice().oscillator;
		};
		const auto adsr = [&]() -> Voice::ADSR & {
			return voice().adsr;
		};

		switch(address.get()) {
			case 0x00:	case 0x07:	case 0x0e:
				oscillator().pitch = (oscillator().pitch & 0xff'00'00) | uint32_t(value << 8);
			break;
			case 0x01:	case 0x08:	case 0x0f:
				oscillator().pitch = (oscillator().pitch & 0x00'ff'00) | uint32_t(value << 16);
			break;
			case 0x02:	case 0x09:	case 0x10:
				oscillator().pulse_width = (oscillator().pitch & 0xf0'00'00'00) | uint32_t(value << 20);
			break;
			case 0x03:	case 0x0a:	case 0x11:
				// The top bit of the phase counter is inverted; since it'll be compared directly with the
				// pulse width, invert that bit too.
				oscillator().pulse_width =
					(
						(oscillator().pitch & 0x0f'f0'00'00) |
						uint32_t(value << 28)
					);
			break;
			case 0x04:	case 0x0b:	case 0x12:
				voice().set_control(value);
			break;
			case 0x05:	case 0x0c:	case 0x13:
				adsr().attack = value >> 4;
				adsr().decay = value;
				adsr().set_phase(adsr().phase);
			break;
			case 0x06:	case 0x0d:	case 0x14:
				adsr().sustain = (value >> 4) | (value & 0xf0);
				adsr().release = value;
				adsr().set_phase(adsr().phase);
			break;

			case 0x15:
				filter_cutoff_.load<0, 3>(value);
				update_filter();
			break;
			case 0x16:
				filter_cutoff_.load<3>(value);
				update_filter();
			break;
			case 0x17:
				filter_channels_ = value;
				filter_resonance_ = value >> 4;
				update_filter();
			break;
			case 0x18:
				volume_ = value & 0x0f;
				filter_mode_ = value >> 4;
				update_filter();
			break;
		}
	});
}

void SID::update_filter() {

}

uint8_t SID::read(const Numeric::SizedInt<5> address) {
	(void)address;
	return last_write_;
}

void SID::set_sample_volume_range(const std::int16_t range) {
	range_ = range;
}

bool SID::is_zero_level() const {
	return false;
}

template <Outputs::Speaker::Action action>
void SID::apply_samples(const std::size_t number_of_samples, Outputs::Speaker::MonoSample *const target) {
	for(std::size_t c = 0; c < number_of_samples; c++) {
		// Advance phase.
		voices_[0].update();
		voices_[1].update();
		voices_[2].update();

		// Apply hard synchronisations.
		voices_[0].synchronise(voices_[2]);
		voices_[1].synchronise(voices_[0]);
		voices_[2].synchronise(voices_[1]);

		// Construct filtered and unfiltered output.
		const uint16_t outputs[3] = {
			voices_[0].output(voices_[2]),
			voices_[1].output(voices_[0]),
			voices_[2].output(voices_[1]),
		};

		const uint16_t direct_sample =
			(filter_channels_.bit<0>() ? 0 : outputs[0]) +
			(filter_channels_.bit<1>() ? 0 : outputs[1]) +
			(filter_channels_.bit<2>() ? 0 : outputs[2]);

		const int16_t filtered_sample =
			filter_.apply(
				(filter_channels_.bit<0>() ? outputs[0] : 0) +
				(filter_channels_.bit<1>() ? outputs[1] : 0) +
				(filter_channels_.bit<2>() ? outputs[2] : 0)
			);

		// Sum, apply volume and output.
		const auto sample =
			(
				volume_ * (
					direct_sample +
					filtered_sample
						- 227	// DC offset.
				)
				- 88732
			) / 3;
		// Maximum range of above: 15 * (4095 * 3 - 227) = [-3405, 180870]
		// So subtracting 88732 will move to the centre of the range, and 3 is the smallest
		// integer that avoids clipping.

		Outputs::Speaker::apply<action>(
			target[c],
			Outputs::Speaker::MonoSample((sample * range_) >> 16)
		);
	}
}

template void SID::apply_samples<Outputs::Speaker::Action::Mix>(
	std::size_t, Outputs::Speaker::MonoSample *);
template void SID::apply_samples<Outputs::Speaker::Action::Store>(
	std::size_t, Outputs::Speaker::MonoSample *);
template void SID::apply_samples<Outputs::Speaker::Action::Ignore>(
	std::size_t, Outputs::Speaker::MonoSample *);
