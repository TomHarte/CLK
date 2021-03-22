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

#include <algorithm>

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
			int contended_period;
			int first_fetch;
			int delays[16];
		};

		static constexpr Timings get_timings() {
			constexpr Timings result = {
				.cycles_per_line = 228 * 2,
				.lines_per_frame = 311,
				.first_delay = 14361 * 2,
				.contended_period = (14490 - 14361) * 2,
				.first_fetch = 14364 * 2,	// TODO: find a source for this, a guess.
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
			constexpr int first_line = timings.first_fetch / timings.cycles_per_line;
			constexpr int sync_position = 166 * 2;
			constexpr int sync_length = 17 * 2;
			constexpr int burst_position = sync_position + 40;
			constexpr int burst_length = 17;

			int cycles_remaining = duration.as<int>();
			while(cycles_remaining) {
				int line = time_since_interrupt_ / timings.cycles_per_line;
				int offset = time_since_interrupt_ % timings.cycles_per_line;
				const int cycles_this_line = std::min(cycles_remaining, timings.cycles_per_line - offset);
				const int end_offset = offset + cycles_this_line;

				if(!offset) {
					is_alternate_line_ ^= true;

					if(!line) {
						flash_counter_ = (flash_counter_ + 1) & 31;
						flash_mask_ = uint8_t(flash_counter_ >> 4);
					}
				}

				if(line < 3) {
					// Output sync line.
					crt_.output_sync(cycles_this_line);
				} else {
					if((line < first_line) || (line >= first_line+192)) {
						// Output plain border line.
						if(offset < sync_position) {
							const int border_duration = std::min(sync_position, end_offset) - offset;
							output_border(border_duration);
							offset += border_duration;
						}
					} else {
						// Output pixel line.
						if(offset < 256) {
							const int pixel_duration = std::min(256, end_offset) - offset;

							if(!offset) {
								const int pixel_line = line - first_line;

								pixel_target_ = crt_.begin_data(256);
								attribute_address_ = ((pixel_line / 8) * 32) + 6144;
								pixel_address_ = ((pixel_line & 0x07) << 8) | ((pixel_line&0x38) << 2) | ((pixel_line&0xc0) << 5);
							}

							if(pixel_target_) {
								const int start_column = offset >> 4;
								const int end_column = (offset + pixel_duration) >> 4;
								for(int column = start_column; column < end_column; column++) {
									const uint8_t attributes[2] = {
										memory_[attribute_address_],
										memory_[attribute_address_+1],
									};

									constexpr uint8_t masks[] = {0, 0xff};
									const uint8_t pixels[2] = {
										uint8_t(memory_[pixel_address_] ^ masks[flash_mask_ & (attributes[0] >> 7)]),
										uint8_t(memory_[pixel_address_+1] ^ masks[flash_mask_ & (attributes[1] >> 7)]),
									};

									{
										const uint8_t colours[2] = {
											palette[(attributes[0] & 0x78) >> 3],
											palette[((attributes[0] & 0x40) >> 3) | (attributes[0] & 0x07)],
										};

										pixel_target_[0] = colours[(pixels[0] >> 7) & 1];
										pixel_target_[1] = colours[(pixels[0] >> 6) & 1];
										pixel_target_[2] = colours[(pixels[0] >> 5) & 1];
										pixel_target_[3] = colours[(pixels[0] >> 4) & 1];
										pixel_target_[4] = colours[(pixels[0] >> 3) & 1];
										pixel_target_[5] = colours[(pixels[0] >> 2) & 1];
										pixel_target_[6] = colours[(pixels[0] >> 1) & 1];
										pixel_target_[7] = colours[(pixels[0] >> 0) & 1];
										pixel_target_ += 8;
									}

									{
										const uint8_t colours[2] = {
											palette[(attributes[1] & 0x78) >> 3],
											palette[((attributes[1] & 0x40) >> 3) | (attributes[1] & 0x07)],
										};

										pixel_target_[0] = colours[(pixels[1] >> 7) & 1];
										pixel_target_[1] = colours[(pixels[1] >> 6) & 1];
										pixel_target_[2] = colours[(pixels[1] >> 5) & 1];
										pixel_target_[3] = colours[(pixels[1] >> 4) & 1];
										pixel_target_[4] = colours[(pixels[1] >> 3) & 1];
										pixel_target_[5] = colours[(pixels[1] >> 2) & 1];
										pixel_target_[6] = colours[(pixels[1] >> 1) & 1];
										pixel_target_[7] = colours[(pixels[1] >> 0) & 1];
										pixel_target_ += 8;
									}

									pixel_address_ += 2;
									attribute_address_ += 2;
								}
							}

							offset += pixel_duration;
							if(offset == 256) {
								crt_.output_data(256);
								pixel_target_ = nullptr;
							}
						}

						if(offset >= 256 && offset < sync_position && end_offset > offset) {
							const int border_duration = std::min(sync_position, end_offset) - offset;
							output_border(border_duration);
							offset += border_duration;
						}
					}

					// Output the common tail to border and pixel lines: sync, blank, colour burst, border.

					if(offset >= sync_position && offset < sync_position + sync_length && end_offset > offset) {
						const int sync_duration = std::min(sync_position + sync_length, end_offset) - offset;
						crt_.output_sync(sync_duration);
						offset += sync_duration;
					}

					if(offset >= sync_position + sync_length && offset < burst_position && end_offset > offset) {
						const int blank_duration = std::min(burst_position, end_offset) - offset;
						crt_.output_blank(blank_duration);
						offset += blank_duration;
					}

					if(offset >= burst_position && offset < burst_position+burst_length && end_offset > offset) {
						const int burst_duration = std::min(burst_position + burst_length, end_offset) - offset;
						crt_.output_colour_burst(burst_duration, 0, is_alternate_line_);
						offset += burst_duration;
					}

					if(offset >= burst_position+burst_length && end_offset > offset) {
						const int border_duration = end_offset - offset;
						output_border(border_duration);
					}
				}

				cycles_remaining -= cycles_this_line;
				time_since_interrupt_ = (time_since_interrupt_ + cycles_this_line) % (timings.cycles_per_line * timings.lines_per_frame);
			}
		}

	private:
		// TODO: how long is the interrupt line held for?
		static constexpr int interrupt_duration = 48;

		void output_border(int duration) {
			uint8_t *const colour_pointer = crt_.begin_data(1);
			if(colour_pointer) *colour_pointer = border_colour_;
			crt_.output_level(duration);
		}

	public:
		Video() :
			crt_(227 * 2, 2, Outputs::Display::Type::PAL50, Outputs::Display::InputDataType::Red2Green2Blue2)
		{
			// Show only the centre 80% of the TV frame.
			crt_.set_display_type(Outputs::Display::DisplayType::RGB);
			crt_.set_visible_area(Outputs::Display::Rect(0.1f, 0.1f, 0.8f, 0.8f));

		}

		void set_video_source(const uint8_t *source) {
			memory_ = source;
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

		int access_delay(HalfCycles offset) const {
			constexpr auto timings = get_timings();
			const int delay_time = (time_since_interrupt_ + offset.as<int>()) % (timings.cycles_per_line * timings.lines_per_frame);

			if(delay_time < timings.first_delay) return 0;

			const int time_since = delay_time - timings.first_delay;
			const int lines = time_since / timings.cycles_per_line;
			if(lines >= 192) return 0;

			const int line_position = time_since % timings.cycles_per_line;
			if(line_position >= timings.contended_period) return 0;

			return timings.delays[line_position & 15];
		}

		void set_border_colour(uint8_t colour) {
			border_colour_ = palette[colour];
		}

		/// Sets the scan target.
		void set_scan_target(Outputs::Display::ScanTarget *scan_target) {
			crt_.set_scan_target(scan_target);
		}

		/// Gets the current scan status.
		Outputs::Display::ScanStatus get_scaled_scan_status() const {
			return crt_.get_scaled_scan_status();
		}

		/*! Sets the type of display the CRT will request. */
		void set_display_type(Outputs::Display::DisplayType type) {
			crt_.set_display_type(type);
		}

	private:
		int time_since_interrupt_ = 0;
		Outputs::CRT::CRT crt_;
		const uint8_t *memory_ = nullptr;
		uint8_t border_colour_ = 0;

		uint8_t *pixel_target_ = nullptr;
		int attribute_address_ = 0;
		int pixel_address_ = 0;

		uint8_t flash_mask_ = 0;
		int flash_counter_ = 0;
		bool is_alternate_line_ = false;

#define RGB(r, g, b)	(r << 4) | (g << 2) | b
		static constexpr uint8_t palette[] = {
			RGB(0, 0, 0),	RGB(0, 0, 2),	RGB(2, 0, 0),	RGB(2, 0, 2),
			RGB(0, 2, 0),	RGB(0, 2, 2),	RGB(2, 2, 0),	RGB(2, 2, 2),
			RGB(0, 0, 0),	RGB(0, 0, 3),	RGB(3, 0, 0),	RGB(3, 0, 3),
			RGB(0, 3, 0),	RGB(0, 3, 3),	RGB(3, 3, 0),	RGB(3, 3, 3),
		};
#undef RGB
};

}
}

#endif /* Video_hpp */
