//
//  Audio.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../Concurrency/AsyncTaskQueue.hpp"
#include "../../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"

#include <array>
#include <cstdint>

namespace Archimedes {

// Generate lookup table for sound output levels, and hold it only once regardless
// of how many template instantiations there are of @c Sound.
static constexpr std::array<int16_t, 256> generate_levels() {
	std::array<int16_t, 256> result{};

	// There are 8 segments of 16 steps; each segment is a linear
	// interpolation from its start level to its end level and
	// each level is double the previous.
	//
	// Bit 7 provides a sign.

	for(size_t c = 0; c < 256; c++) {
		// This is the VIDC1 rule.
//		const bool is_negative = c & 128;
//		const auto point = static_cast<int>(c & 0xf);
//		const auto chord = static_cast<int>((c >> 4) & 7);

		// VIDC2 rule, which seems to be effective. I've yet to spot the rule by which
		// VIDC1/2 is detected.
		const bool is_negative = c & 1;
		const auto point = static_cast<int>((c >> 1) & 0xf);
		const auto chord = static_cast<int>((c >> 5) & 7);

		const int start = (1 << chord) - 1;
		const int end = (chord == 7) ? 247 : ((start << 1) + 1);

		const int level = start * (16 - point) + end * point;
		result[c] = static_cast<int16_t>((level * 32767) / 3832);
		if(is_negative) result[c] = -result[c];
	}

	return result;
}
struct SoundLevels {
	static constexpr auto levels = generate_levels();
};

/// Models the Archimedes sound output; in a real machine this is a joint efort between the VIDC and the MEMC.
template <typename InterruptObserverT>
struct Sound: private SoundLevels {
	Sound(InterruptObserverT &observer, const uint8_t *ram) : ram_(ram), observer_(observer) {
		speaker_.set_input_rate(1'000'000);
		speaker_.set_high_frequency_cutoff(2'200.0f);
	}

	void set_next_end(uint32_t value) {
		next_.end = value;
	}

	void set_next_start(uint32_t value) {
		next_.start = value;
		set_buffer_valid(true);	// My guess: this is triggered on next buffer start write.

		// Definitely wrong; testing.
//		set_halted(false);
	}

	bool interrupt() const {
		return !next_buffer_valid_;
	}

	void swap() {
		current_.start = next_.start;
		std::swap(current_.end, next_.end);
		set_buffer_valid(false);
		set_halted(false);
	}

	void set_frequency(uint8_t frequency) {
		divider_ = reload_ = frequency;
	}

	void set_stereo_image(uint8_t channel, uint8_t value) {
		if(!value) {
			positions_[channel].left =
			positions_[channel].right = 0;
			return;
		}

		positions_[channel].right = value - 1;
		positions_[channel].left = 6 - positions_[channel].right;
	}

	void set_dma_enabled(bool enabled) {
		dma_enabled_ = enabled;
	}

	void tick() {
		// Write silence if not currently outputting.
		if(halted_ || !dma_enabled_) {
			post_sample(Outputs::Speaker::StereoSample());
			return;
		}

		// Apply user-programmed clock divider.
		--divider_;
		if(!divider_) {
			divider_ = reload_ + 2;

			// Grab a single byte from the FIFO.
			const uint8_t raw = ram_[static_cast<std::size_t>(current_.start) + static_cast<std::size_t>(byte_)];
			sample_ = Outputs::Speaker::StereoSample(	// TODO: pan, volume.
				static_cast<int16_t>((levels[raw] * positions_[byte_ & 7].left) / 6),
				static_cast<int16_t>((levels[raw] * positions_[byte_ & 7].right) / 6)
			);
			++byte_;

			// If the FIFO is exhausted, consider triggering a DMA request.
			if(byte_ == 16) {
				byte_ = 0;

				current_.start += 16;
				if(current_.start == current_.end) {
					if(next_buffer_valid_) {
						swap();
					} else {
						set_halted(true);
					}
				}
			}
		}
		post_sample(sample_);
	}

	Outputs::Speaker::Speaker *speaker() {
		return &speaker_;
	}

	~Sound() {
		while(is_posting_.test_and_set());
	}

private:
	const uint8_t *ram_ = nullptr;

	uint8_t divider_ = 0, reload_ = 0;
	int byte_ = 0;

	void set_buffer_valid(bool valid) {
		next_buffer_valid_ = valid;
		observer_.update_interrupts();
	}

	void set_halted(bool halted) {
		if(halted_ != halted && !halted) {
			byte_ = 0;
			divider_ = reload_;
		}
		halted_ = halted;
	}

	bool next_buffer_valid_ = false;
	bool halted_ = true;				// This is a bit of a guess.
	bool dma_enabled_ = false;

	struct Buffer {
		uint32_t start = 0, end = 0;
	};
	Buffer current_, next_;

	struct StereoPosition {
		// These are maintained as sixths, i.e. a value of 6 means 100%.
		int left, right;
	} positions_[8];

	InterruptObserverT &observer_;
	Outputs::Speaker::PushLowpass<true> speaker_;
	Concurrency::AsyncTaskQueue<true> queue_;

	void post_sample(Outputs::Speaker::StereoSample sample) {
		samples_[sample_target_][sample_pointer_++] = sample;
		if(sample_pointer_ == samples_[0].size()) {
			while(is_posting_.test_and_set());

			const auto post_source = sample_target_;
			sample_target_ ^= 1;
			sample_pointer_ = 0;
			queue_.enqueue([this, post_source]() {
				speaker_.push(reinterpret_cast<int16_t *>(samples_[post_source].data()), samples_[post_source].size());
				is_posting_.clear();
			});
		}
	}
	std::size_t sample_pointer_ = 0;
	std::size_t sample_target_ = 0;
	Outputs::Speaker::StereoSample sample_;

	using SampleBuffer = std::array<Outputs::Speaker::StereoSample, 4096>;
	std::array<SampleBuffer, 2> samples_;
	std::atomic_flag is_posting_ = ATOMIC_FLAG_INIT;
};

}
