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
			// Number of cycles per line. Will be 224 or 228.
			int cycles_per_line;
			// Number of lines comprising a whole frame. Will be 311 or 312.
			int lines_per_frame;

			// Number of cycles after first pixel fetch at which interrupt is first signalled.
			int interrupt_time;

			// Number of cycles before first pixel fetch that contention starts to be applied.
			int contention_leadin;
			// Period in a line for which contention is applied.
			int contention_duration;

			// Contention to apply, in half-cycles, as a function of number of half cycles since
			// contention began.
			int delays[16];
		};

		static constexpr Timings get_timings() {
			// Amstrad gate array timings, classic statement:
			//
			// Contention begins 14361 cycles "after interrupt" and follows the pattern [1, 0, 7, 6 5 4, 3, 2].
			// The first four bytes of video are fetched at 14365–14368 cycles, in the order [pixels, attribute, pixels, attribute].
			//
			// For my purposes:
			//
			// Video fetching always begins at 0. Since there are 311*228 = 70908 cycles per frame, and the interrupt
			// should "occur" (I assume: begin) 14365 before that, it should actually begin at 70908 - 14365 = 56543.
			//
			// Contention begins four cycles before the first video fetch, so it begins at 70904. I don't currently
			// know whether the four cycles is true across all models, so it's given here as convention_leadin.
			//
			// ... except that empirically that all seems to be two cycles off. So maybe I misunderstand what the
			// contention patterns are supposed to indicate relative to MREQ? It's frustrating that all documentation
			// I can find is vaguely in terms of contention patterns, and what they mean isn't well-defined in terms
			// of regular Z80 signalling.
			constexpr Timings result = {
				.cycles_per_line = 228 * 2,
				.lines_per_frame = 311,

				.interrupt_time = 56542 * 2,

				// i.e. video fetching begins five cycles after the start of the
				// contended memory pattern below; that should put a clear two
				// cycles between a Z80 access and the first video fetch.
				.contention_leadin = 5 * 2,
				.contention_duration = 129 * 2,

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

		// TODO: how long is the interrupt line held for?
		static constexpr int interrupt_duration = 48;

	public:
		void run_for(HalfCycles duration) {
			constexpr auto timings = get_timings();

			constexpr int sync_line = (timings.interrupt_time / timings.cycles_per_line) + 1;

			constexpr int sync_position = 166 * 2;
			constexpr int sync_length = 17 * 2;
			constexpr int burst_position = sync_position + 40;
			constexpr int burst_length = 17;

			int cycles_remaining = duration.as<int>();
			while(cycles_remaining) {
				int line = time_into_frame_ / timings.cycles_per_line;
				int offset = time_into_frame_ % timings.cycles_per_line;
				const int cycles_this_line = std::min(cycles_remaining, timings.cycles_per_line - offset);
				const int end_offset = offset + cycles_this_line;

				if(!offset) {
					is_alternate_line_ ^= true;

					if(!line) {
						flash_counter_ = (flash_counter_ + 1) & 31;
						flash_mask_ = uint8_t(flash_counter_ >> 4);
					}
				}

				if(line >= sync_line && line < sync_line + 3) {
					// Output sync line.
					crt_.output_sync(cycles_this_line);
				} else {
					if(line >= 192) {
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
								pixel_target_ = crt_.begin_data(256);
								attribute_address_ = ((line >> 3) << 5) + 6144;
								pixel_address_ = ((line & 0x07) << 8) | ((line & 0x38) << 2) | ((line & 0xc0) << 5);
							}

							if(pixel_target_) {
								const int start_column = offset >> 4;
								const int end_column = (offset + pixel_duration) >> 4;
								for(int column = start_column; column < end_column; column++) {
									last_fetches_[0] = memory_[pixel_address_];
									last_fetches_[1] = memory_[attribute_address_];
									last_fetches_[2] = memory_[pixel_address_+1];
									last_fetches_[3] = memory_[attribute_address_+1];
									set_last_contended_area_access(last_fetches_[3]);

									pixel_address_ += 2;
									attribute_address_ += 2;

									constexpr uint8_t masks[] = {0, 0xff};

#define Output(n)	\
	{				\
		const uint8_t pixels =														\
			uint8_t(last_fetches_[n] ^ masks[flash_mask_ & (last_fetches_[n+1] >> 7)]);	\
			\
		const uint8_t colours[2] = {													\
			palette[(last_fetches_[n+1] & 0x78) >> 3],									\
			palette[((last_fetches_[n+1] & 0x40) >> 3) | (last_fetches_[n+1] & 0x07)],	\
		};	\
			\
		pixel_target_[0] = colours[(pixels >> 7) & 1];	\
		pixel_target_[1] = colours[(pixels >> 6) & 1];	\
		pixel_target_[2] = colours[(pixels >> 5) & 1];	\
		pixel_target_[3] = colours[(pixels >> 4) & 1];	\
		pixel_target_[4] = colours[(pixels >> 3) & 1];	\
		pixel_target_[5] = colours[(pixels >> 2) & 1];	\
		pixel_target_[6] = colours[(pixels >> 1) & 1];	\
		pixel_target_[7] = colours[(pixels >> 0) & 1];	\
		pixel_target_ += 8;									\
	}

									Output(0);
									Output(2);

#undef Output
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
						crt_.output_colour_burst(burst_duration, 116, is_alternate_line_);
						offset += burst_duration;
						// The colour burst phase above is an empirical guess. I need to research further.
					}

					if(offset >= burst_position+burst_length && end_offset > offset) {
						const int border_duration = end_offset - offset;
						output_border(border_duration);
					}
				}

				cycles_remaining -= cycles_this_line;
				time_into_frame_ = (time_into_frame_ + cycles_this_line) % (timings.cycles_per_line * timings.lines_per_frame);
			}
		}

	private:
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

		/*!
			@returns The amount of time until the next change in the interrupt line, that being the only internally-observeable output.
		*/
		HalfCycles get_next_sequence_point() {
			constexpr auto timings = get_timings();

			// Is the frame still ahead of this interrupt?
			if(time_into_frame_ < timings.interrupt_time) {
				return HalfCycles(timings.interrupt_time - time_into_frame_);
			}

			// If not, is it within this interrupt?
			if(time_into_frame_ < timings.interrupt_time + interrupt_duration) {
				return HalfCycles(timings.interrupt_time + interrupt_duration - time_into_frame_);
			}

			// If not, it'll be in the next batch.
			return timings.interrupt_time + timings.cycles_per_line * timings.lines_per_frame - time_into_frame_;
		}

		/*!
			@returns The current state of the interrupt output.
		*/
		bool get_interrupt_line() const {
			constexpr auto timings = get_timings();
			return time_into_frame_ >= timings.interrupt_time && time_into_frame_ < timings.interrupt_time + interrupt_duration;
		}

		/*!
			@returns How many cycles the [ULA/gate array] would delay the CPU for if it were to recognise that contention
			needs to be applied in @c offset half-cycles from now.
		*/
		int access_delay(HalfCycles offset) const {
			constexpr auto timings = get_timings();
			const int delay_time = (time_into_frame_ + offset.as<int>() + timings.contention_leadin) % (timings.cycles_per_line * timings.lines_per_frame);

			// Check for a time within the no-contention window.
			if(delay_time >= (191*timings.cycles_per_line + timings.contention_duration)) {
				return 0;
			}

			const int time_into_line = delay_time % timings.cycles_per_line;
			if(time_into_line >= timings.contention_duration) {
				return 0;
			}

			return timings.delays[time_into_line & 15];
		}

		/*!
			@returns Whatever the ULA or gate array would expose via the floating bus, this cycle.
		*/
		uint8_t get_floating_value() const {
			constexpr auto timings = get_timings();
			const int line = time_into_frame_ / timings.cycles_per_line;
			if(line >= 192) return 0xff;

			const int time_into_line = time_into_frame_ % timings.cycles_per_line;
			if(time_into_line >= 256 || (time_into_line&8)) {
				return last_contended_access_;
			}

			// The +2a and +3 always return the low bit as set.
			if constexpr (timing == VideoTiming::Plus3) {
				return last_fetches_[(time_into_line >> 1) & 3] | 1;
			}

			return last_fetches_[(time_into_line >> 1) & 3];
		}

		/*!
			Relevant to the +2a and +3 only, sets the most recent value read from or
			written to contended memory. This is what will be returned if the floating
			bus is accessed when the gate array isn't currently reading.
		*/
		void set_last_contended_area_access([[maybe_unused]] uint8_t value) {
			if constexpr (timing == VideoTiming::Plus3) {
				last_contended_access_ = value | 1;
			}
		}

		/*!
			Sets the current border colour.
		*/
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
		int time_into_frame_ = 0;
		Outputs::CRT::CRT crt_;
		const uint8_t *memory_ = nullptr;
		uint8_t border_colour_ = 0;

		uint8_t *pixel_target_ = nullptr;
		int attribute_address_ = 0;
		int pixel_address_ = 0;

		uint8_t flash_mask_ = 0;
		int flash_counter_ = 0;
		bool is_alternate_line_ = false;

		uint8_t last_fetches_[4] = {0xff, 0xff, 0xff, 0xff};
		uint8_t last_contended_access_ = 0xff;

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
