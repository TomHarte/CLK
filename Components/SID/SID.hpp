//
//  SID.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/11/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Numeric/SizedInt.hpp"

#include "Concurrency/AsyncTaskQueue.hpp"
#include "Outputs/Speaker/Implementation/BufferSource.hpp"

namespace MOS::SID {

class SID: public Outputs::Speaker::BufferSource<SID, false> {
public:
	SID(Concurrency::AsyncTaskQueue<false> &audio_queue);

	void write(Numeric::SizedInt<5> address, uint8_t value);
	uint8_t read(Numeric::SizedInt<5> address);

	// Outputs::Speaker::BufferSource.
	template <Outputs::Speaker::Action action>
		void apply_samples(std::size_t number_of_samples, Outputs::Speaker::MonoSample *target);
	bool is_zero_level() const;
	void set_sample_volume_range(std::int16_t);

private:
	Concurrency::AsyncTaskQueue<false> &audio_queue_;

	struct Voice {
		Numeric::SizedInt<16> pitch;
		Numeric::SizedInt<12> pulse_width;
		Numeric::SizedInt<8> control;
		Numeric::SizedInt<4> attack;
		Numeric::SizedInt<4> decay;
		Numeric::SizedInt<4> sustain;
		Numeric::SizedInt<4> release;

		bool noise() const { return control.bit<7>(); }
		bool pulse() const { return control.bit<6>(); }
		bool sawtooth() const { return control.bit<5>(); }
		bool triangle() const { return control.bit<4>(); }
		bool test() const { return control.bit<3>(); }
		bool ring_mod() const { return control.bit<2>(); }
		bool sync() const { return control.bit<1>(); }
		bool gate() const { return control.bit<0>(); }

		Numeric::SizedInt<24> phase;

		void update() {
			phase += pitch.get();
		}
	};

	Voice voices_[3];
};

}
