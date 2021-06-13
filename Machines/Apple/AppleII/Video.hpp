//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef Apple_II_Video_hpp
#define Apple_II_Video_hpp

#include "../../../Outputs/CRT/CRT.hpp"
#include "../../../ClockReceiver/ClockReceiver.hpp"
#include "../../../ClockReceiver/DeferredQueue.hpp"

#include "VideoSwitches.hpp"

#include <array>
#include <vector>

namespace Apple {
namespace II {
namespace Video {

class BusHandler {
	public:
		/*!
			Requests fetching of the @c count bytes starting from @c address.

			The handler should write the values from base memory to @c base_target, and those
			from auxiliary memory to @c auxiliary_target. If the machine has no axiliary memory,
			it needn't write anything to auxiliary_target.
		*/
		void perform_read([[maybe_unused]] uint16_t address, [[maybe_unused]] size_t count, [[maybe_unused]] uint8_t *base_target, [[maybe_unused]] uint8_t *auxiliary_target) {
		}
};

class VideoBase: public VideoSwitches<Cycles> {
	public:
		VideoBase(bool is_iie, std::function<void(Cycles)> &&target);

		/// Sets the scan target.
		void set_scan_target(Outputs::Display::ScanTarget *scan_target);

		/// Gets the current scan status.
		Outputs::Display::ScanStatus get_scaled_scan_status() const;

		/// Sets the type of output.
		void set_display_type(Outputs::Display::DisplayType);

		/// Gets the type of output.
		Outputs::Display::DisplayType get_display_type() const;

		/// Sets whether the current CRT should be recalibrated away from normative NTSC
		/// to produce square pixels in 40-column text mode.
		void set_use_square_pixels(bool);
		bool get_use_square_pixels();

	protected:
		Outputs::CRT::CRT crt_;
		bool use_square_pixels_ = false;

		// State affecting output video stream generation.
		uint8_t *pixel_pointer_ = nullptr;

		// State affecting logical state.
		int row_ = 0, column_ = 0;

		// Graphics carry is the final level output in a fetch window;
		// it carries on into the next if it's high resolution with
		// the delay bit set.
		mutable uint8_t graphics_carry_ = 0;
		bool was_double_ = false;

		// Memory is fetched ahead of time into this array;
		// this permits the correct delay between fetching
		// without having to worry about a rolling buffer.
		std::array<uint8_t, 40> base_stream_;
		std::array<uint8_t, 40> auxiliary_stream_;

		const bool is_iie_ = false;

		/*!
			Outputs 40-column text to @c target, using @c length bytes from @c source.
		*/
		void output_text(uint8_t *target, const uint8_t *source, size_t length, size_t pixel_row) const;

		/*!
			Outputs 80-column text to @c target, drawing @c length columns from @c source and @c auxiliary_source.
		*/
		void output_double_text(uint8_t *target, const uint8_t *source, const uint8_t *auxiliary_source, size_t length, size_t pixel_row) const;

		/*!
			Outputs 40-column low-resolution graphics to @c target, drawing @c length columns from @c source.
		*/
		void output_low_resolution(uint8_t *target, const uint8_t *source, size_t length, int column, int row) const;

		/*!
			Outputs 80-column low-resolution graphics to @c target, drawing @c length columns from @c source and @c auxiliary_source.
		*/
		void output_double_low_resolution(uint8_t *target, const uint8_t *source, const uint8_t *auxiliary_source, size_t length, int column, int row) const;

		/*!
			Outputs 40-column high-resolution graphics to @c target, drawing @c length columns from @c source.
		*/
		void output_high_resolution(uint8_t *target, const uint8_t *source, size_t length) const;

		/*!
			Outputs 80-column double-high-resolution graphics to @c target, drawing @c length columns from @c source.
		*/
		void output_double_high_resolution(uint8_t *target, const uint8_t *source, const uint8_t *auxiliary_source, size_t length) const;

		/*!
			Outputs 40-column "fat low resolution" graphics to @c target, drawing @c length columns from @c source.

			Fat low-resolution mode is like regular low-resolution mode except that data is shifted out on the 7M
			clock rather than the 14M.
		*/
		void output_fat_low_resolution(uint8_t *target, const uint8_t *source, size_t length, int column, int row) const;
};

template <class BusHandler, bool is_iie> class Video: public VideoBase {
	public:
		/// Constructs an instance of the video feed; a CRT is also created.
		Video(BusHandler &bus_handler) :
			VideoBase(is_iie, [this] (Cycles cycles) { advance(cycles); }),
			bus_handler_(bus_handler) {}

		/*!
			Obtains the last value the video read prior to time now+offset.
		*/
		uint8_t get_last_read_value(Cycles offset) {
			// Rules of generation:
			// (1)	a complete sixty-five-cycle scan line consists of sixty-five consecutive bytes of
			//		display buffer memory that starts twenty-five bytes prior to the actual data to be displayed.
			// (2)	During VBL the data acts just as if it were starting a whole new frame from the beginning, but
			//		it never finishes this pseudo-frame. After getting one third of the way through the frame (to
			//		scan line $3F), it suddenly repeats the previous six scan lines ($3A through $3F) before aborting
			//		to begin the next true frame.
			//
			// Source: Have an Apple Split by Bob Bishop; http://rich12345.tripod.com/aiivideo/softalk.html

			// Determine column at offset.
			int mapped_column = column_ + int(offset.as_integral());

			// Map that backwards from the internal pixels-at-start generation to pixels-at-end
			// (so what was column 0 is now column 25).
			mapped_column += 25;

			// Apply carry into the row counter.
			int mapped_row = row_ + (mapped_column / 65);
			mapped_column %= 65;
			mapped_row %= 262;

			// Apple out-of-bounds row logic.
			if(mapped_row >= 256) {
				mapped_row = 0x3a + (mapped_row&255);
			} else {
				mapped_row %= 192;
			}

			// Calculate the address and return the value.
			uint16_t read_address = uint16_t(get_row_address(mapped_row) + mapped_column - 25);
			uint8_t value, aux_value;
			bus_handler_.perform_read(read_address, 1, &value, &aux_value);
			return value;
		}

		/*!
			@returns @c true if the display will be within vertical blank at now + @c offset; @c false otherwise.
		*/
		bool get_is_vertical_blank(Cycles offset) {
			// Map that backwards from the internal pixels-at-start generation to pixels-at-end
			// (so what was column 0 is now column 25).
			int mapped_column = column_ + int(offset.as_integral());

			// Map that backwards from the internal pixels-at-start generation to pixels-at-end
			// (so what was column 0 is now column 25).
			mapped_column += 25;

			// Apply carry into the row counter and test it for location.
			int mapped_row = row_ + (mapped_column / 65);

			// Per http://www.1000bit.it/support/manuali/apple/technotes/iigs/tn.iigs.040.html
			// "on the IIe, the screen is blanked when the bit is low".
			return (mapped_row % 262) < 192;
		}

	private:
		/*!
			Advances time by @c cycles; expects to be fed by the CPU clock.
			Implicitly adds an extra half a colour clock at the end of
			line.
		*/
		void advance(Cycles cycles) {
			/*
				Addressing scheme used throughout is that column 0 is the first column with pixels in it;
				row 0 is the first row with pixels in it.

				A frame is oriented around 65 cycles across, 262 lines down.
			*/
			constexpr int first_sync_line = 220;	// A complete guess. Information needed.
			constexpr int first_sync_column = 49;	// Also a guess.
			constexpr int sync_length = 4;			// One of the two likely candidates.

			int int_cycles = int(cycles.as_integral());
			while(int_cycles) {
				const int cycles_this_line = std::min(65 - column_, int_cycles);
				const int ending_column = column_ + cycles_this_line;
				const bool is_vertical_sync_line = (row_ >= first_sync_line && row_ < first_sync_line + 3);

				if(is_vertical_sync_line) {
					// In effect apply an XOR to HSYNC and VSYNC flags in order to include equalising
					// pulses (and hencce keep hsync approximately where it should be during vsync).
					const int blank_start = std::max(first_sync_column - sync_length, column_);
					const int blank_end = std::min(first_sync_column, ending_column);
					if(blank_end > blank_start) {
						if(blank_start > column_) {
							crt_.output_sync((blank_start - column_) * 14);
						}
						crt_.output_blank((blank_end - blank_start) * 14);
						if(blank_end < ending_column) {
							crt_.output_sync((ending_column - blank_end) * 14);
						}
					} else {
						crt_.output_sync(cycles_this_line * 14);
					}
				} else {
					const GraphicsMode line_mode = graphics_mode(row_);

					// Determine whether there's any fetching to do. Fetching occurs during the first
					// 40 columns of rows prior to 192.
					if(row_ < 192 && column_ < 40) {
						const int character_row = row_ >> 3;
						const uint16_t row_address = uint16_t((character_row >> 3) * 40 + ((character_row&7) << 7));

						// Grab the memory contents that'll be needed momentarily.
						const int fetch_end = std::min(40, ending_column);
						uint16_t fetch_address;
						switch(line_mode) {
							default:
							case GraphicsMode::Text:
							case GraphicsMode::DoubleText:
							case GraphicsMode::LowRes:
							case GraphicsMode::FatLowRes:
							case GraphicsMode::DoubleLowRes: {
								const uint16_t text_address = uint16_t(((video_page()+1) * 0x400) + row_address);
								fetch_address = uint16_t(text_address + column_);
							} break;

							case GraphicsMode::HighRes:
							case GraphicsMode::DoubleHighRes:
								fetch_address = uint16_t(((video_page()+1) * 0x2000) + row_address + ((row_&7) << 10) + column_);
							break;
						}

						bus_handler_.perform_read(
							fetch_address,
							size_t(fetch_end - column_),
							&base_stream_[size_t(column_)],
							&auxiliary_stream_[size_t(column_)]);
					}

					if(row_ < 192) {
						// The pixel area is the first 40.5 columns; base contents
						// remain where they would naturally be but auxiliary
						// graphics appear to the left of that.
						if(!column_) {
							pixel_pointer_ = crt_.begin_data(568);
							graphics_carry_ = 0;
							was_double_ = true;
						}

						if(column_ < 40) {
							const int pixel_start = std::max(0, column_);
							const int pixel_end = std::min(40, ending_column);
							const int pixel_row = row_ & 7;

							const bool is_double = is_double_mode(line_mode);
							if(!is_double && was_double_ && pixel_pointer_) {
								pixel_pointer_[pixel_start*14 + 0] =
								pixel_pointer_[pixel_start*14 + 1] =
								pixel_pointer_[pixel_start*14 + 2] =
								pixel_pointer_[pixel_start*14 + 3] =
								pixel_pointer_[pixel_start*14 + 4] =
								pixel_pointer_[pixel_start*14 + 5] =
								pixel_pointer_[pixel_start*14 + 6] = 0;
							}
							was_double_ = is_double;

							if(pixel_pointer_) {
								switch(line_mode) {
									case GraphicsMode::Text:
										output_text(
											&pixel_pointer_[pixel_start * 14 + 7],
											&base_stream_[size_t(pixel_start)],
											size_t(pixel_end - pixel_start),
											size_t(pixel_row));
									break;

									case GraphicsMode::DoubleText:
										output_double_text(
											&pixel_pointer_[pixel_start * 14],
											&base_stream_[size_t(pixel_start)],
											&auxiliary_stream_[size_t(pixel_start)],
											size_t(pixel_end - pixel_start),
											size_t(pixel_row));
									break;

									case GraphicsMode::LowRes:
										output_low_resolution(
											&pixel_pointer_[pixel_start * 14 + 7],
											&base_stream_[size_t(pixel_start)],
											size_t(pixel_end - pixel_start),
											pixel_start,
											pixel_row);
									break;

									case GraphicsMode::FatLowRes:
										output_fat_low_resolution(
											&pixel_pointer_[pixel_start * 14 + 7],
											&base_stream_[size_t(pixel_start)],
											size_t(pixel_end - pixel_start),
											pixel_start,
											pixel_row);
									break;

									case GraphicsMode::DoubleLowRes:
										output_double_low_resolution(
											&pixel_pointer_[pixel_start * 14],
											&base_stream_[size_t(pixel_start)],
											&auxiliary_stream_[size_t(pixel_start)],
											size_t(pixel_end - pixel_start),
											pixel_start,
											pixel_row);
									break;

									case GraphicsMode::HighRes:
										output_high_resolution(
											&pixel_pointer_[pixel_start * 14 + 7],
											&base_stream_[size_t(pixel_start)],
											size_t(pixel_end - pixel_start));
									break;

									case GraphicsMode::DoubleHighRes:
										output_double_high_resolution(
											&pixel_pointer_[pixel_start * 14],
											&base_stream_[size_t(pixel_start)],
											&auxiliary_stream_[size_t(pixel_start)],
											size_t(pixel_end - pixel_start));
									break;

									default: break;
								}
							}

							if(pixel_end == 40) {
								if(pixel_pointer_) {
									if(was_double_) {
										pixel_pointer_[560] = pixel_pointer_[561] = pixel_pointer_[562] = pixel_pointer_[563] =
										pixel_pointer_[564] = pixel_pointer_[565] = pixel_pointer_[566] = pixel_pointer_[567] = 0;
									} else {
										if(line_mode == GraphicsMode::HighRes && base_stream_[39]&0x80)
											pixel_pointer_[567] = graphics_carry_;
										else
											pixel_pointer_[567] = 0;
									}
								}

								crt_.output_data(568, 568);
								pixel_pointer_ = nullptr;
							}
						}
					} else {
						if(column_ < 40 && ending_column >= 40) {
							crt_.output_blank(568);
						}
					}

					/*
						The left border, sync, right border pattern doesn't depend on whether
						there were pixels this row and is output as soon as it is known.
					*/

					if(column_ < first_sync_column && ending_column >= first_sync_column) {
						crt_.output_blank(first_sync_column*14 - 568);
					}

					if(column_ < (first_sync_column + sync_length) && ending_column >= (first_sync_column + sync_length)) {
						crt_.output_sync(sync_length*14);
					}

					int second_blank_start;
					if(!is_text_mode(graphics_mode(row_+1))) {
						const int colour_burst_start = std::max(first_sync_column + sync_length + 1, column_);
						const int colour_burst_end = std::min(first_sync_column + sync_length + 4, ending_column);
						if(colour_burst_end > colour_burst_start) {
							// UGLY HACK AHOY!
							// The OpenGL scan target introduces a phase error of 1/8th of a wave. The Metal one does not.
							// Supply the real phase value if this is an Apple build.
							// TODO: eliminate UGLY HACK.
#if defined(__APPLE__) && !defined(IGNORE_APPLE)
							constexpr int phase = 224;
#else
							constexpr int phase = 0;
#endif

							crt_.output_colour_burst((colour_burst_end - colour_burst_start) * 14, phase);
						}

						second_blank_start = std::max(first_sync_column + sync_length + 3, column_);
					} else {
						second_blank_start = std::max(first_sync_column + sync_length, column_);
					}

					if(ending_column > second_blank_start) {
						crt_.output_blank((ending_column - second_blank_start) * 14);
					}
				}

				int_cycles -= cycles_this_line;
				column_ = (column_ + cycles_this_line) % 65;
				if(!column_) {
					row_ = (row_ + 1) % 262;
					did_end_line();

					// Add an extra half a colour cycle of blank; this isn't counted in the run_for
					// count explicitly but is promised. If this is a vertical sync line, output sync
					// instead of blank, taking that to be the default level.
					if(is_vertical_sync_line) {
						crt_.output_sync(2);
					} else {
						crt_.output_blank(2);
					}
				}
			}
		}

		BusHandler &bus_handler_;
};

}
}
}

#endif /* Apple_II_Video_hpp */
