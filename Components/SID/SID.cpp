//
//  SID.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/11/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include "SID.hpp"

// Sources used:
//
//	(1) SID Article v0.2 at https://github.com/ImreOlajos/SID-Article
//	(2) Technical SID Information/Software stuff at http://www.sidmusic.org/sid/sidtech2.html
//	(3) SID 6581/8580 (Sound Interface Device) reference at https://oxyron.de/html/registers_sid.html

using namespace MOS::SID;

SID::SID(Concurrency::AsyncTaskQueue<false> &audio_queue) :
	audio_queue_(audio_queue),
	output_filter_(
		SignalProcessing::BiquadFilter::Type::LowPass,
		1000000.0f,
		15000.0f
	) {}

// MARK: - Programmer interface.

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

void SID::set_potentometer_input(const int index, const uint8_t value) {
	potentometers_[index] = value;
}

void SID::update_filter() {
	using Type = SignalProcessing::BiquadFilter::Type;
	Type type = Type::AllPass;

	switch(filter_mode_.get()) {
		case 0:
			filter_ = SignalProcessing::BiquadFilter();
		return;

		case 1:
		case 3: type = Type::LowPass;	break;

		case 2: type = Type::BandPass;	break;
		case 5: type = Type::Notch;		break;

		case 4:
		case 6: type = Type::HighPass;	break;

		case 7: type = Type::AllPass;	break;
	}

	filter_.configure(
		type,
		1'000'000.0f,
		30.0f + float(filter_cutoff_.get()) * 5.8f,
		0.707f + float(filter_resonance_.get()) * 0.2862f,
		6.0f,
		true
	);

	// Filter cutoff: the data sheet provides that it is linear, and "approximate Cutoff Frequency
	// ranges between 30Hz and 12KHz [with recommended externally-supplied capacitors]."
	//
	// It's an 11-bit number, so the above is "approximate"ly right.

	// Resonance: a complete from-thin-air guess. The data sheet says merely:
	//
	//	"There are 16 Resonance settings ranging from about 0.707 (Critical Damping) for a count of 0
	//	to a maximum for a count of 15"
	//
	// i.e. no information is given on the maximum. I've taken it to be 5-ish per commentary on more general sites
	// that 5 is a typical ceiling for the resonance factor.
}

uint8_t SID::read(const Numeric::SizedInt<5> address) {
	switch(address.get()) {
		default:	return last_write_;

		case 0x19:	return potentometers_[0];
		case 0x1a:	return potentometers_[1];

		case 0x1b:
		case 0x1c:
			// Ensure all channels are entirely up to date.
			audio_queue_.spin_flush();
			return (address == 0x1c) ? voices_[2].adsr.envelope : uint8_t(voices_[2].output(voices_[1]) >> 4);
	}
}

// MARK: - Oscillators.

void Voice::Oscillator::reset_phase() {
	phase = PhaseReload;
}

bool Voice::Oscillator::did_raise_b23() const {
	return previous_phase > phase;
}

bool Voice::Oscillator::did_raise_b19() const {
	static constexpr int NoiseBit = 1 << (19 + 8);
	return (previous_phase ^ phase) & phase & NoiseBit;
}

uint16_t Voice::Oscillator::sawtooth_output() const {
	return (phase >> 20) ^ 0x800;
}

// MARK: - Noise generator.

uint16_t Voice::NoiseGenerator::output() const {
	// Uses bits: 20, 18, 14, 11, 9, 5, 2 and 0, plus four more zero bits.
	const uint16_t output =
		((noise >> 9) & 0b1000'0000'0000) |		// b20 -> b11
		((noise >> 8) & 0b0100'0000'0000) |		// b18 -> b10
		((noise >> 5) & 0b0010'0000'0000) |		// b14 -> b9
		((noise >> 3) & 0b0001'0000'0000) |		// b11 -> b8
		((noise >> 2) & 0b0000'1000'0000) |		// b9 -> b7
		((noise << 1) & 0b0000'0100'0000) |		// b5 -> b6
		((noise << 3) & 0b0000'0010'0000) |		// b2 -> b5
		((noise << 4) & 0b0000'0001'0000);		// b0 -> b4

	assert(output <= Voice::MaxWaveformValue);
	return output;
}

void Voice::NoiseGenerator::update(const bool test) {
	noise =
		(noise << 1) |
		(((noise >> 17) ^ ((noise >> 22) | test)) & 1);
}

// MARK: - ADSR.

void Voice::ADSR::set_phase(const Phase new_phase) {
	static constexpr uint16_t rate_prescaler[] = {
		9, 32, 63, 95, 149, 220, 267, 313, 392, 977, 1954, 3126, 3907, 11720, 19532, 31251
	};
	static_assert(sizeof(rate_prescaler) / sizeof(*rate_prescaler) == 16);

	phase = new_phase;
	switch(phase) {
		case Phase::Attack:			rate_counter_target = rate_prescaler[attack.get()]; break;
		case Phase::DecayAndHold:	rate_counter_target = rate_prescaler[decay.get()]; break;
		case Phase::Release:		rate_counter_target = rate_prescaler[release.get()]; break;
	}
}

// MARK: - Voices.

void Voice::set_control(const uint8_t new_control) {
	const bool old_gate = gate();
	control = new_control;
	if(gate() && !old_gate) {
		adsr.set_phase(ADSR::Phase::Attack);
	} else if(!gate() && old_gate) {
		adsr.set_phase(ADSR::Phase::Release);
	}
}

bool Voice::noise() const		{ return control.bit<7>(); }
bool Voice::pulse() const		{ return control.bit<6>(); }
bool Voice::sawtooth() const	{ return control.bit<5>(); }
bool Voice::triangle() const	{ return control.bit<4>(); }
bool Voice::test() const		{ return control.bit<3>(); }
bool Voice::ring_mod() const	{ return control.bit<2>(); }
bool Voice::sync() const		{ return control.bit<1>(); }
bool Voice::gate() const		{ return control.bit<0>(); }

void Voice::update() {
	// Oscillator.
	oscillator.previous_phase = oscillator.phase;
	if(test()) {
		oscillator.phase = 0;
	} else {
		oscillator.phase += oscillator.pitch;

		if(oscillator.did_raise_b19()) {
			noise_generator.update(test());
		}
	}

	// ADSR.

	// First prescalar, which is a function of the programmer-set rate.
	++ adsr.rate_counter;
	if(adsr.rate_counter == adsr.rate_counter_target) {
		adsr.rate_counter = 0;

		// Second prescalar, which approximates an exponential.
		static constexpr uint8_t exponential_prescaler[] = {
			1,														// 0
			30, 30, 30, 30, 30, 30,									// 1–6
			16, 16, 16, 16, 16, 16, 16, 16,							// 7–14
			8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,						// 15–26
			4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
			4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,				// 27–54
			2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
			2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
			2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,					// 55–94
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1,
		};
		static_assert(sizeof(exponential_prescaler) == 256);
		static_assert(exponential_prescaler[0] == 1);
		static_assert(exponential_prescaler[1] == 30);
		static_assert(exponential_prescaler[6] == 30);
		static_assert(exponential_prescaler[7] == 16);
		static_assert(exponential_prescaler[14] == 16);
		static_assert(exponential_prescaler[15] == 8);
		static_assert(exponential_prescaler[26] == 8);
		static_assert(exponential_prescaler[27] == 4);
		static_assert(exponential_prescaler[54] == 4);
		static_assert(exponential_prescaler[55] == 2);
		static_assert(exponential_prescaler[94] == 2);
		static_assert(exponential_prescaler[95] == 1);
		static_assert(exponential_prescaler[255] == 1);

		if(adsr.phase == ADSR::Phase::Attack) {
			++adsr.envelope;
			// TODO: what really resets the exponential counter? If anything?
			adsr.exponential_counter = 0;

			if(adsr.envelope == 0xff) {
				adsr.set_phase(ADSR::Phase::DecayAndHold);
			}
		} else {
			++adsr.exponential_counter;
			if(adsr.exponential_counter == exponential_prescaler[adsr.envelope]) {
				adsr.exponential_counter = 0;

				if(adsr.envelope && (adsr.envelope != adsr.sustain || adsr.phase != ADSR::Phase::DecayAndHold)) {
					--adsr.envelope;
				}
			}
		}
	}
}

void Voice::synchronise(const Voice &prior) {
	// Only oscillator work to do here.
	if(
		sync() &&
		prior.oscillator.did_raise_b23()
	) {
		oscillator.phase = Oscillator::PhaseReload;
	}
}

uint16_t Voice::pulse_output() const {
	return (
		(oscillator.phase ^ 0x8000'0000) < oscillator.pulse_width
	) ? 0 : MaxWaveformValue;
}

uint16_t Voice::triangle_output(const Voice &prior) const {
	const uint16_t sawtooth = oscillator.sawtooth_output();
	const uint16_t xor_mask1 = sawtooth;
	const uint16_t xor_mask2 = ring_mod() ? prior.sawtooth() : 0;
	const uint16_t xor_mask = ((xor_mask1 ^ xor_mask2) & 0x800) ? 0xfff : 0x000;
	return ((sawtooth << 1) ^ xor_mask) & 0xfff;
}

uint16_t Voice::output(const Voice &prior) const {
	// TODO: true composite waves.
	//
	// My current understanding on this: if multiple waveforms are enabled, the pull to zero beats the
	// pull to one on any line where the two compete. But the twist is that the lines are not necessarily
	// one per bit since they lead to a common ground. Ummm, I think.
	//
	// Anyway, first pass: logical AND. It's not right. It will temporarily do.

	uint16_t output = MaxWaveformValue;

	if(pulse())		output &= pulse_output();
	if(sawtooth())	output &= oscillator.sawtooth_output();
	if(triangle())	output &= triangle_output(prior);
	if(noise())		output &= noise_generator.output();

	return (output * adsr.envelope) / 255;
}

// MARK: - Wave generation

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
		const auto sample = output_filter_.apply(int16_t(
			(
				volume_ * (
					direct_sample +
					filtered_sample
						- 227	// DC offset.
				)
				- 88732
			) / 3
		));
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
