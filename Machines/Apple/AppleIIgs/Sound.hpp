//
//  Sound.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/11/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#pragma once

#include <atomic>

#include "../../../ClockReceiver/ClockReceiver.hpp"
#include "../../../Concurrency/AsyncTaskQueue.hpp"
#include "../../../Outputs/Speaker/Implementation/SampleSource.hpp"

namespace Apple::IIgs::Sound {

class GLU: public Outputs::Speaker::SampleSource {
	public:
		GLU(Concurrency::AsyncTaskQueue<false> &audio_queue);

		void set_control(uint8_t);
		uint8_t get_control();
		void set_data(uint8_t);
		uint8_t get_data();
		void set_address_low(uint8_t);
		uint8_t get_address_low();
		void set_address_high(uint8_t);
		uint8_t get_address_high();

		void run_for(Cycles);
		Cycles next_sequence_point() const;
		bool get_interrupt_line();

		// SampleSource.
		void get_samples(std::size_t number_of_samples, std::int16_t *target);
		void set_sample_volume_range(std::int16_t range);
		void skip_samples(const std::size_t number_of_samples);

	private:
		Concurrency::AsyncTaskQueue<false> &audio_queue_;

		uint16_t address_ = 0;

		// Use a circular buffer for piping memory alterations onto the audio
		// thread; it would be prohibitive to defer every write individually.
		//
		// Assumed: on most modern architectures, an atomic 64-bit read or
		// write can be achieved locklessly.
		struct MemoryWrite {
			uint32_t time;
			uint16_t address;
			uint8_t value;
			bool enabled;
		};
		static_assert(sizeof(MemoryWrite) == 8);
		constexpr static int StoreBufferSize = 16384;

		std::atomic<MemoryWrite> pending_stores_[StoreBufferSize];
		uint32_t pending_store_read_ = 0, pending_store_read_time_ = 0;
		uint32_t pending_store_write_ = 0, pending_store_write_time_ = 0;

		// Maintain state both 'locally' (i.e. on the emulation thread) and
		// 'remotely' (i.e. on the audio thread).
		struct EnsoniqState {
			uint8_t ram_[65536];
			struct Oscillator {
				uint32_t position;

				// Programmer-set values.
				uint16_t velocity;
				uint8_t volume;
				uint8_t address;
				uint8_t control;
				uint8_t table_size;

				// Derived state.
				uint32_t overflow_mask;			// If a non-zero bit gets anywhere into the overflow mask, this channel
												// has wrapped around. It's a function of table_size.
				bool interrupt_request = false;	// Will be non-zero if this channel would request an interrupt, were
												// it currently enabled to do so.

				uint8_t sample(uint8_t *ram);
				int16_t output(uint8_t *ram);
			} oscillators[32];

			// Some of these aren't actually needed on both threads.
			uint8_t control = 0;
			int oscillator_count = 1;

			void set_register(uint16_t address, uint8_t value);
		} local_, remote_;

		// Functions to update an EnsoniqState; these don't belong to the state itself
		// because they also access the pending stores (inter alia).
		void generate_audio(size_t number_of_samples, std::int16_t *target);
		void skip_audio(EnsoniqState &state, size_t number_of_samples);

		// Audio-thread state.
		int16_t output_range_ = 0;
};

}
