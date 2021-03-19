//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/03/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Video_hpp
#define Video_hpp

#include "../../../Outputs/CRT/CRT.hpp"
#include "../../../ClockReceiver/ClockReceiver.hpp"

namespace Sinclair {
namespace ZXSpectrum {

enum class VideoTiming {
	Plus3
};

/*
	Timing notes:

	As of the +2a/+3:

		311 lines, 228 cycles/line
		Delays begin at 14361, follow the pattern 1, 0, 7, 6, 5, 4, 3, 2; run for 129 cycles/line.
		Possibly delays only affect actual reads and writes; documentation is unclear.

	Unknowns, to me, presently:

		How long the interrupt line held for.

	So...

		Probably two bytes of video and attribute are fetched in each 8-cycle block,
		with 16 such blocks therefore providing the whole visible display, an island
		within 28.5 blocks horizontally.

		14364 is 228*63, so I I guess almost 63 lines run from the start of vertical
		blank through to the top of the display, implying 56 lines on to vertical blank.

*/

template <VideoTiming timing> class Video {
	private:
		struct Timings {
			int cycles_per_line;
			int lines_per_frame;
			int first_delay;
			int first_border;
			int delays[16];
		};

		static constexpr Timings get_timings() {
			constexpr Timings result = {
				.cycles_per_line = 228 * 2,
				.lines_per_frame = 311,
				.first_delay = 14361 * 2,
				.first_border = 14490 * 2,
				.delays = {
					2, 1,
					0, 0,
					14, 13,
					12, 11,
					10, 9,
					8, 7,
					6, 5,
					4, 3,
				}
			};
			return result;
		}

	public:
		void run_for(HalfCycles duration) {
			constexpr auto timings = get_timings();

			// Advance time. TODO: all the drawing.
			time_since_interrupt_ = (time_since_interrupt_ + duration.as<int>()) % (timings.cycles_per_line * timings.lines_per_frame);
		}

	private:
		// TODO: how long is the interrupt line held for?
		static constexpr int interrupt_duration = 48;

	public:
		Video() :
			crt_(227 * 2, 1, Outputs::Display::Type::PAL50, Outputs::Display::InputDataType::Red2Green2Blue2)
		{
			// Show only the centre 80% of the TV frame.
			crt_.set_display_type(Outputs::Display::DisplayType::RGB);
			crt_.set_visible_area(Outputs::Display::Rect(0.1f, 0.1f, 0.8f, 0.8f));

		}

		HalfCycles get_next_sequence_point() {
			if(time_since_interrupt_ < interrupt_duration) {
				return HalfCycles(interrupt_duration - time_since_interrupt_);
			}

			constexpr auto timings = get_timings();
			return timings.cycles_per_line * timings.lines_per_frame - time_since_interrupt_;
		}

		bool get_interrupt_line() const {
			return time_since_interrupt_ < interrupt_duration;
		}

		int access_delay() const {
			constexpr auto timings = get_timings();
			if(time_since_interrupt_ < timings.first_delay) return 0;

			const int time_since = time_since_interrupt_ - timings.first_delay;
			const int lines = time_since / timings.cycles_per_line;
			if(lines >= 192) return 0;

			const int line_position = time_since % timings.cycles_per_line;
			if(line_position >= timings.first_border - timings.first_delay) return 0;

			return timings.delays[line_position & 7];
		}

		/// Sets the scan target.
		void set_scan_target(Outputs::Display::ScanTarget *scan_target) {
			crt_.set_scan_target(scan_target);
		}

		/// Gets the current scan status.
		Outputs::Display::ScanStatus get_scaled_scan_status() const {
			return crt_.get_scaled_scan_status();
		}

	private:
		int time_since_interrupt_ = 0;
		Outputs::CRT::CRT crt_;
};

}
}

#endif /* Video_hpp */
