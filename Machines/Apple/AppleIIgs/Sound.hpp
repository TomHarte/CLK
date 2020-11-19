//
//  Sound.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/11/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef Apple_IIgs_Sound_hpp
#define Apple_IIgs_Sound_hpp

#include "../../../ClockReceiver/ClockReceiver.hpp"
#include "../../../Concurrency/AsyncTaskQueue.hpp"
#include "../../../Outputs/Speaker/Implementation/SampleSource.hpp"

namespace Apple {
namespace IIgs {
namespace Sound {

class GLU: public Outputs::Speaker::SampleSource {
	public:
		GLU(Concurrency::DeferringAsyncTaskQueue &audio_queue);

		void set_control(uint8_t);
		uint8_t get_control();
		void set_data(uint8_t);
		uint8_t get_data();
		void set_address_low(uint8_t);
		uint8_t get_address_low();
		void set_address_high(uint8_t);
		uint8_t get_address_high();

		void run_for(Cycles);

		// SampleSource.
		void get_samples(std::size_t number_of_samples, std::int16_t *target);
		void set_sample_volume_range(std::int16_t range);
		void skip_samples(const std::size_t number_of_samples);

	private:
		Concurrency::DeferringAsyncTaskQueue &audio_queue_;

		uint16_t address_ = 0;

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
			} oscillators_[32];

			uint8_t control;

			void generate_audio(size_t number_of_samples, std::int16_t *target, int16_t range);
			void skip_audio(size_t number_of_samples);
		} local_, remote_;
		uint8_t interrupt_state_;
		uint8_t oscillator_enable_;

		uint8_t control_ = 0x00;

		int16_t output_range_ = 0;
};

}
}
}

#endif /* SoundGLU_hpp */
