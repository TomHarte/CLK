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

#include "../../../Reflection/Struct.hpp"

#include <algorithm>

namespace Sinclair {
namespace ZXSpectrum {
namespace Video {

enum class Timing {
	FortyEightK,
	OneTwoEightK,
	Plus3,
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

template <Timing timing> class Video {
	private:
		struct Timings {
			// Number of cycles per line. Will be 224 or 228.
			int cycles_per_line;
			// Number of lines comprising a whole frame. Will be 311 or 312.
			int lines_per_frame;

			// Number of cycles before first pixel fetch that contention starts to be applied.
			int contention_leadin;
			// Period in a line for which contention is applied.
			int contention_duration;

			// Number of cycles after first pixel fetch at which interrupt is first signalled.
			int interrupt_time;

			// Contention to apply, in whole cycles, as a function of number of whole cycles since
			// contention began.
			int delays[8];

			constexpr Timings(int cycles_per_line, int lines_per_frame, int contention_leadin, int contention_duration, int interrupt_offset, const int *delays) noexcept :
				cycles_per_line(cycles_per_line * 2),
				lines_per_frame(lines_per_frame),
				contention_leadin(contention_leadin * 2),
				contention_duration(contention_duration * 2),
				interrupt_time((cycles_per_line * lines_per_frame - interrupt_offset - contention_leadin) * 2),
				delays{ delays[0] * 2, delays[1] * 2, delays[2] * 2, delays[3] * 2, delays[4] * 2, delays[5] * 2, delays[6] * 2, delays[7] * 2}
			 {}
		};

		static constexpr Timings get_timings() {
			if constexpr (timing == Timing::Plus3) {
				constexpr int delays[] = {1, 0, 7, 6, 5, 4, 3, 2};
				return Timings(228, 311, 6, 129, 14361, delays);
			}

			if constexpr (timing == Timing::OneTwoEightK) {
				constexpr int delays[] = {6, 5, 4, 3, 2, 1, 0, 0};
				return Timings(228, 311, 4, 128, 14361, delays);
			}

			if constexpr (timing == Timing::FortyEightK) {
				constexpr int delays[] = {6, 5, 4, 3, 2, 1, 0, 0};
				return Timings(224, 312, 4, 128, 14335, delays);
			}
		}

		// Interrupt should be held for 32 cycles.
		static constexpr int interrupt_duration = 64;

	public:
		void run_for(HalfCycles duration) {
			constexpr auto timings = get_timings();

			constexpr int sync_line = (timings.interrupt_time / timings.cycles_per_line) + 1;

			constexpr int sync_position = (timing == Timing::FortyEightK) ? 164 * 2 : 166 * 2;
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

						if constexpr (timing >= Timing::OneTwoEightK) {
							crt_.output_colour_burst(burst_duration, 116, is_alternate_line_);
							// The colour burst phase above is an empirical guess. I need to research further.
						} else {
							crt_.output_default_colour_burst(burst_duration);
						}
						offset += burst_duration;
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

		static constexpr int half_cycles_per_line() {
			if constexpr (timing == Timing::FortyEightK) {
				// TODO: determine real figure here, if one exists.
				// The source I'm looking at now suggests that the theoretical
				// ideal of 224*2 ignores the real-life effects of separate
				// crystals, so I've nudged this experimentally.
				return 224*2 - 1;
			} else {
				return 227*2;
			}
		}

	public:
		Video() :
			crt_(half_cycles_per_line(), 2, Outputs::Display::Type::PAL50, Outputs::Display::InputDataType::Red2Green2Blue2)
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
		HalfCycles access_delay(HalfCycles offset) const {
			constexpr auto timings = get_timings();
			const int delay_time = (time_into_frame_ + offset.as<int>() + timings.contention_leadin) % (timings.cycles_per_line * timings.lines_per_frame);
			assert(!(delay_time&1));

			// Check for a time within the no-contention window.
			if(delay_time >= (191*timings.cycles_per_line + timings.contention_duration)) {
				return 0;
			}

			const int time_into_line = delay_time % timings.cycles_per_line;
			if(time_into_line >= timings.contention_duration) {
				return 0;
			}

			return HalfCycles(timings.delays[(time_into_line >> 1) & 7]);
		}

		/*!
			@returns Whatever the ULA or gate array would expose via the floating bus, this cycle.
		*/
		uint8_t get_floating_value() const {
			constexpr auto timings = get_timings();
			const uint8_t out_of_bounds = (timing == Timing::Plus3) ? last_contended_access_ : 0xff;

			const int line = time_into_frame_ / timings.cycles_per_line;
			if(line >= 192) {
				return out_of_bounds;
			}

			const int time_into_line = time_into_frame_ % timings.cycles_per_line;
			if(time_into_line >= 256 || (time_into_line&8)) {
				return out_of_bounds;
			}

			// The +2a and +3 always return the low bit as set.
			const uint8_t value = last_fetches_[(time_into_line >> 1) & 3];
			if constexpr (timing == Timing::Plus3) {
				return value | 1;
			}
			return value;
		}

		/*!
			Relevant to the +2a and +3 only, sets the most recent value read from or
			written to contended memory. This is what will be returned if the floating
			bus is accessed when the gate array isn't currently reading.
		*/
		void set_last_contended_area_access([[maybe_unused]] uint8_t value) {
			if constexpr (timing == Timing::Plus3) {
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

struct State: public Reflection::StructImpl<State> {
	uint8_t border_colour;

	State() {
		if(needs_declare()) {
			DeclareField(border_colour);
		}
	}
};

}
}
}

#endif /* Video_hpp */
