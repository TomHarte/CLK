//
//  SN76489.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/02/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../Outputs/Speaker/Implementation/BufferSource.hpp"
#include "../../Concurrency/AsyncTaskQueue.hpp"

namespace TI {

class SN76489: public Outputs::Speaker::BufferSource<SN76489, false> {
	public:
		enum class Personality {
			SN76489,
			SN76494,
			SMS
		};

		/// Creates a new SN76489.
		SN76489(Personality personality, Concurrency::AsyncTaskQueue<false> &task_queue, int additional_divider = 1);

		/// Writes a new value to the SN76489.
		void write(uint8_t value);

		// As per SampleSource.
		template <Outputs::Speaker::Action action>
		void apply_samples(std::size_t number_of_samples, Outputs::Speaker::MonoSample *target);
		bool is_zero_level() const;
		void set_sample_volume_range(std::int16_t range);

	private:
		int master_divider_ = 0;
		int master_divider_period_ = 16;
		int16_t output_volume_ = 0;
		void evaluate_output_volume();
		int volumes_[16];

		Concurrency::AsyncTaskQueue<false> &task_queue_;

		struct ToneChannel {
			// Programmatically-set state; updated by the processor.
			uint16_t divider = 0;
			uint8_t volume = 0xf;

			// Active state; self-evolving as a function of time.
			uint16_t counter = 0;
			int level = 0;
		} channels_[4];
		enum {
			Periodic15,
			Periodic16,
			Noise15,
			Noise16
		} noise_mode_ = Periodic15;
		uint16_t noise_shifter_ = 0;
		int active_register_ = 0;

		bool shifter_is_16bit_ = false;
};

}
