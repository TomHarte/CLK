//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/06/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Machines_ZX8081_Video_hpp
#define Machines_ZX8081_Video_hpp

#include "../../Outputs/CRT/CRT.hpp"
#include "../../ClockReceiver/ClockReceiver.hpp"

namespace ZX8081 {

/*!
	Packages a ZX80/81-style video feed into a CRT-compatible waveform.

	While sync is active, this feed will output the sync level.

	While sync is inactive, this feed will output the white level unless it is supplied
	with a byte to output. When a byte is supplied for output, it will be interpreted as
	a 1-bit graphic and output over the next 4 cycles, picking between the white level
	and the black level.
*/
class Video {
	public:
		/// Constructs an instance of the video feed; a CRT is also created.
		Video();
		/// @returns The CRT this video feed is feeding.
		Outputs::CRT::CRT *get_crt();

		/// Advances time by @c half-cycles.
		void run_for(const HalfCycles);
		/// Forces output to catch up to the current output position.
		void flush();

		/// Sets the current sync output.
		void set_sync(bool sync);
		/// Causes @c byte to be serialised into pixels and output over the next four cycles.
		void output_byte(uint8_t byte);

	private:
		bool sync_ = false;
		uint8_t *line_data_ = nullptr;
		uint8_t *line_data_pointer_ = nullptr;
		unsigned int cycles_since_update_ = 0;
		std::unique_ptr<Outputs::CRT::CRT> crt_;

		void flush(bool next_sync);
};

}

#endif /* Video_hpp */
