//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef Video_hpp
#define Video_hpp

#include "../../Outputs/CRT/CRT.hpp"
#include "../../ClockReceiver/ClockReceiver.hpp"

#include <array>
#include <vector>

namespace AppleII {
namespace Video {

class BusHandler {
	public:
		/*!
			Requests fetching of the @c count bytes starting from @c address.

			The handler should write the values from base memory to @c base_target, and those
			from auxiliary memory to @c auxiliary_target. If the machine has no axiliary memory,
			it needn't write anything to auxiliary_target.
		*/
		void perform_read(uint16_t address, size_t count, uint8_t *base_target, uint8_t *auxiliary_target) {
		}
};

class VideoBase {
	public:
		VideoBase(bool is_iie);

		/// @returns The CRT this video feed is feeding.
		Outputs::CRT::CRT *get_crt();

		/*
			Descriptions for the setters below are taken verbatim from
			the Apple IIe Technical Reference. Addresses are the conventional
			locations within the Apple II memory map. Only those which affect
			video output are implemented here.

			Those registers which don't exist on a II/II+ are marked.
		*/

		/*!
			Setter for ALTCHAR ($C00E/$C00F; triggers on write only):

			* Off: display text using primary character set.
			* On: display text using alternate character set.

			Doesn't exist on a II/II+.
		*/
		void set_alternative_character_set(bool);
		bool get_alternative_character_set();

		/*!
			Setter for 80COL ($C00C/$C00D; triggers on write only).

			* Off: display 40 columns.
			* On: display 80 columns.

			Doesn't exist on a II/II+.
		*/
		void set_80_columns(bool);
		bool get_80_columns();

		/*!
			Setter for 80STORE ($C000/$C001; triggers on write only).

			* Off: cause PAGE2 to select auxiliary RAM.
			* On: cause PAGE2 to switch main RAM areas.

			Doesn't exist on a II/II+.
		*/
		void set_80_store(bool);
		bool get_80_store();

		/*!
			Setter for PAGE2 ($C054/$C055; triggers on read or write).

			* Off: select Page 1.
			* On: select Page 2 or, if 80STORE on, Page 1 in auxiliary memory.

			80STORE doesn't exist on a II/II+; therefore this always selects
			either Page 1 or Page 2 on those machines.
		*/
		void set_page2(bool);
		bool get_page2();

		/*!
			Setter for TEXT ($C050/$C051; triggers on read or write).

			* Off: display graphics or, if MIXED on, mixed.
			* On: display text.
		*/
		void set_text(bool);
		bool get_text();

		/*!
			Setter for MIXED ($C052/$C053; triggers on read or write).

			* Off: display only text or only graphics.
			* On: if TEXT off, display text and graphics.
		*/
		void set_mixed(bool);
		bool get_mixed();

		/*!
			Setter for HIRES ($C056/$C057; triggers on read or write).

			* Off: if TEXT off, display low-resolution graphics.
			* On: if TEXT off, display high-resolution or, if DHIRES on, double high-resolution graphics.

			DHIRES doesn't exist on a II/II+; therefore this always selects
			either high- or low-resolution graphics on those machines.

			Despite Apple's documentation, the IIe also supports double low-resolution
			graphics, which are the 80-column analogue to ordinary low-resolution 40-column
			low-resolution graphics.
		*/
		void set_high_resolution(bool);
		bool get_high_resolution();

		/*!
			Setter for DHIRES ($C05E/$C05F; triggers on write only).

			* On: turn on double-high resolution.
			* Off: turn off double-high resolution.

			DHIRES doesn't exist on a II/II+. On the IIe there is another
			register usually grouped with the graphics setters called IOUDIS
			that affects visibility of this switch. But it has no effect on
			video, so it's not modelled by this class.
		*/
		void set_double_high_resolution(bool);
		bool get_double_high_resolution();

		// Setup for text mode.
		void set_character_rom(const std::vector<uint8_t> &);

	protected:
		std::unique_ptr<Outputs::CRT::CRT> crt_;

		// State affecting output video stream generation.
		uint8_t *pixel_pointer_ = nullptr;
		int pixel_pointer_column_ = 0;
		bool pixels_are_high_density_ = false;

		// State affecting logical state.
		int row_ = 0, column_ = 0, flash_ = 0;

		// Enumerates all Apple II and IIe display modes.
		enum class GraphicsMode {
			LowRes,
			DoubleLowRes,
			HighRes,
			DoubleHighRes,
			Text,
			DoubleText
		};
		bool is_text_mode(GraphicsMode m) { return m >= GraphicsMode::Text; }

		// Various soft-switch values.
		bool alternative_character_set_ = false;
		bool columns_80_ = false;
		bool store_80_ = false;
		bool page2_ = false;
		bool text_ = true;
		bool mixed_ = false;
		bool high_resolution_ = false;
		bool double_high_resolution_ = false;

		// Graphics carry is the final level output in a fetch window;
		// it carries on into the next if it's high resolution with
		// the delay bit set.
		uint8_t graphics_carry_ = 0;

		// This holds a copy of the character ROM. The regular character
		// set is assumed to be in the first 64*8 bytes; the alternative
		// is in the 128*8 bytes after that.
		std::vector<uint8_t> character_rom_;

		// Memory is fetched ahead of time into this array;
		// this permits the correct delay between fetching
		// without having to worry about a rolling buffer.
		std::array<uint8_t, 40> base_stream_;
		std::array<uint8_t, 40> auxiliary_stream_;

		bool is_iie_ = false;
		static const int flash_length = 8406;

		/*!
			Outputs 40-column text to @c target, using @c length bytes from @c source.
			@return One byte after the final value written to @c target.
		*/
		uint8_t *output_text(uint8_t *target, uint8_t *source, size_t length, size_t pixel_row);

		/*!
			Outputs 80-column text to @c target, drawing @c length columns from @c source and @c auxiliary_source.
			@return One byte after the final value written to @c target.
		*/
		uint8_t *output_double_text(uint8_t *target, uint8_t *source, uint8_t *auxiliary_source, size_t length, size_t pixel_row);

		/*!
			Outputs 40-column low-resolution graphics to @c target, drawing @c length columns from @c source.
			@return One byte after the final value written to @c target.
		*/
		uint8_t *output_low_resolution(uint8_t *target, uint8_t *source, size_t length, int row);

		/*!
			Outputs 80-column low-resolution graphics to @c target, drawing @c length columns from @c source and @c auxiliary_source.
			@return One byte after the final value written to @c target.
		*/
		uint8_t *output_double_low_resolution(uint8_t *target, uint8_t *source, uint8_t *auxiliary_source, size_t length, int row);

		/*!
			Outputs 40-column high-resolution graphics to @c target, drawing @c length columns from @c source.
			@return One byte after the final value written to @c target.
		*/
		uint8_t *output_high_resolution(uint8_t *target, uint8_t *source, size_t length);

		/*!
			Outputs 80-column double-high-resolution graphics to @c target, drawing @c length columns from @c source.
			@return One byte after the final value written to @c target.
		*/
		uint8_t *output_double_high_resolution(uint8_t *target, uint8_t *source, uint8_t *auxiliary_source, size_t length);
};

template <class BusHandler, bool is_iie> class Video: public VideoBase {
	public:
		/// Constructs an instance of the video feed; a CRT is also created.
		Video(BusHandler &bus_handler) :
			VideoBase(is_iie),
			bus_handler_(bus_handler) {}

		/*!
			Advances time by @c cycles; expects to be fed by the CPU clock.
			Implicitly adds an extra half a colour clock at the end of every
			line.
		*/
		void run_for(const Cycles cycles) {
			/*
				Addressing scheme used throughout is that column 0 is the first column with pixels in it;
				row 0 is the first row with pixels in it.

				A frame is oriented around 65 cycles across, 262 lines down.
			*/
			static const int first_sync_line = 220;		// A complete guess. Information needed.
			static const int first_sync_column = 49;	// Also a guess.
			static const int sync_length = 4;			// One of the two likely candidates.

			int int_cycles = cycles.as_int();
			while(int_cycles) {
				const int cycles_this_line = std::min(65 - column_, int_cycles);
				const int ending_column = column_ + cycles_this_line;

				if(row_ >= first_sync_line && row_ < first_sync_line + 3) {
					// In effect apply an XOR to HSYNC and VSYNC flags in order to include equalising
					// pulses (and hencce keep hsync approximately where it should be during vsync).
					const int blank_start = std::max(first_sync_column - sync_length, column_);
					const int blank_end = std::min(first_sync_column, ending_column);
					if(blank_end > blank_start) {
						if(blank_start > column_) {
							crt_->output_sync(static_cast<unsigned int>(blank_start - column_) * 14);
						}
						crt_->output_blank(static_cast<unsigned int>(blank_end - blank_start) * 14);
						if(blank_end < ending_column) {
							crt_->output_sync(static_cast<unsigned int>(ending_column - blank_end) * 14);
						}
					} else {
						crt_->output_sync(static_cast<unsigned int>(cycles_this_line) * 14);
					}
				} else {
					const GraphicsMode line_mode = graphics_mode(row_);

					// The first 40 columns are submitted to the CRT only upon completion;
					// they'll be either graphics or blank, depending on which side we are
					// of line 192.
					if(column_ < 40) {
						if(row_ < 192) {
							const bool requires_high_density = line_mode != GraphicsMode::Text;
							if(!column_ || requires_high_density != pixels_are_high_density_) {
								if(column_) output_data_to_column(column_);
								pixel_pointer_ = crt_->allocate_write_area(561);
								pixel_pointer_column_ = column_;
								pixels_are_high_density_ = requires_high_density;
								graphics_carry_ = 0;
							}

							const int pixel_end = std::min(40, ending_column);
							const int character_row = row_ >> 3;
							const int pixel_row = row_ & 7;
							const uint16_t row_address = static_cast<uint16_t>((character_row >> 3) * 40 + ((character_row&7) << 7));
							const uint16_t text_address = static_cast<uint16_t>(((video_page()+1) * 0x400) + row_address);

							// Grab the memory contents that'll be needed momentarily.
							switch(line_mode) {
								case GraphicsMode::Text:
								case GraphicsMode::DoubleText:
								case GraphicsMode::LowRes:
								case GraphicsMode::DoubleLowRes:
									bus_handler_.perform_read(text_address + column_, pixel_end - column_, &base_stream_[column_], &auxiliary_stream_[column_]);
									// TODO: should character modes be mapped to character pixel outputs here?
								break;

								case GraphicsMode::HighRes:
								case GraphicsMode::DoubleHighRes:
									bus_handler_.perform_read(static_cast<uint16_t>(((video_page()+1) * 0x2000) + row_address + ((pixel_row&7) << 10)) + column_, pixel_end - column_, &base_stream_[column_], &auxiliary_stream_[column_]);
								break;
							}

							switch(line_mode) {
								case GraphicsMode::Text:
									pixel_pointer_ = output_text(
										pixel_pointer_,
										&base_stream_[static_cast<size_t>(column_)],
										static_cast<size_t>(pixel_end - column_),
										static_cast<size_t>(pixel_row));
								break;

								case GraphicsMode::DoubleText:
									pixel_pointer_ = output_double_text(
										pixel_pointer_,
										&base_stream_[static_cast<size_t>(column_)],
										&auxiliary_stream_[static_cast<size_t>(column_)],
										static_cast<size_t>(pixel_end - column_),
										static_cast<size_t>(pixel_row));
								break;

								case GraphicsMode::LowRes:
									pixel_pointer_ = output_low_resolution(
										pixel_pointer_,
										&base_stream_[static_cast<size_t>(column_)],
										static_cast<size_t>(pixel_end - column_),
										pixel_row);
								break;

								case GraphicsMode::DoubleLowRes:
									pixel_pointer_ = output_double_low_resolution(
										pixel_pointer_,
										&base_stream_[static_cast<size_t>(column_)],
										&auxiliary_stream_[static_cast<size_t>(column_)],
										static_cast<size_t>(pixel_end - column_),
										pixel_row);
								break;

								case GraphicsMode::HighRes:
									pixel_pointer_ = output_high_resolution(
										pixel_pointer_,
										&base_stream_[static_cast<size_t>(column_)],
										static_cast<size_t>(pixel_end - column_));
								break;

								case GraphicsMode::DoubleHighRes:
									pixel_pointer_ = output_double_high_resolution(
										pixel_pointer_,
										&base_stream_[static_cast<size_t>(column_)],
										&auxiliary_stream_[static_cast<size_t>(column_)],
										static_cast<size_t>(pixel_end - column_));
								break;
							}

							if(ending_column >= 40) {
								output_data_to_column(40);
							}
						} else {
							if(ending_column >= 40) {
								crt_->output_blank(560);
							}
						}
					}

					/*
						The left border, sync, right border pattern doesn't depend on whether
						there were pixels this row and is output as soon as it is known.
					*/

					const int first_blank_start = std::max(40, column_);
					const int first_blank_end = std::min(first_sync_column, ending_column);
					if(first_blank_end > first_blank_start) {
						crt_->output_blank(static_cast<unsigned int>(first_blank_end - first_blank_start) * 14);
					}

					const int sync_start = std::max(first_sync_column, column_);
					const int sync_end = std::min(first_sync_column + sync_length, ending_column);
					if(sync_end > sync_start) {
						crt_->output_sync(static_cast<unsigned int>(sync_end - sync_start) * 14);
					}

					int second_blank_start;
					if(!is_text_mode(graphics_mode(row_+1))) {
						const int colour_burst_start = std::max(first_sync_column + sync_length + 1, column_);
						const int colour_burst_end = std::min(first_sync_column + sync_length + 4, ending_column);
						if(colour_burst_end > colour_burst_start) {
							crt_->output_colour_burst(static_cast<unsigned int>(colour_burst_end - colour_burst_start) * 14, 128);
						}

						second_blank_start = std::max(first_sync_column + 7, column_);
					} else {
						second_blank_start = std::max(first_sync_column + 4, column_);
					}

					if(ending_column > second_blank_start) {
						crt_->output_blank(static_cast<unsigned int>(ending_column - second_blank_start) * 14);
					}
				}

				int_cycles -= cycles_this_line;
				column_ = (column_ + cycles_this_line) % 65;
				if(!column_) {
					row_ = (row_ + 1) % 262;
					flash_ = (flash_ + 1) % (2 * flash_length);

					// Add an extra half a colour cycle of blank; this isn't counted in the run_for
					// count explicitly but is promised.
					crt_->output_blank(2);
				}
			}
		}

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
			int mapped_column = column_ + offset.as_int();

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
			uint16_t read_address = static_cast<uint16_t>(get_row_address(mapped_row) + mapped_column - 25);
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
			int mapped_column = column_ + offset.as_int();

			// Map that backwards from the internal pixels-at-start generation to pixels-at-end
			// (so what was column 0 is now column 25).
			mapped_column += 25;

			// Apply carry into the row counter and test it for location.
			int mapped_row = row_ + (mapped_column / 65);
			return (mapped_row % 262) >= 192;
		}

	private:
		GraphicsMode graphics_mode(int row) {
			if(text_) return columns_80_ ? GraphicsMode::DoubleText : GraphicsMode::Text;
			if(mixed_ && row >= 160 && row < 192) {
				return (columns_80_ || double_high_resolution_) ? GraphicsMode::DoubleText : GraphicsMode::Text;
			}
			if(high_resolution_) {
				return double_high_resolution_ ? GraphicsMode::DoubleHighRes : GraphicsMode::HighRes;
			} else {
				return double_high_resolution_ ? GraphicsMode::DoubleLowRes : GraphicsMode::LowRes;
			}
		}

		int video_page() {
			return (store_80_ || !page2_) ? 0 : 1;
		}

		uint16_t get_row_address(int row) {
			const int character_row = row >> 3;
			const int pixel_row = row & 7;
			const uint16_t row_address = static_cast<uint16_t>((character_row >> 3) * 40 + ((character_row&7) << 7));

			const GraphicsMode pixel_mode = graphics_mode(row);
			return ((pixel_mode == GraphicsMode::HighRes) || (pixel_mode == GraphicsMode::DoubleHighRes)) ?
				static_cast<uint16_t>(((video_page()+1) * 0x2000) + row_address + ((pixel_row&7) << 10)) :
				static_cast<uint16_t>(((video_page()+1) * 0x400) + row_address);
		}

		BusHandler &bus_handler_;
		void output_data_to_column(int column) {
			int length = column - pixel_pointer_column_;
			crt_->output_data(static_cast<unsigned int>(length*14), static_cast<unsigned int>(length * (pixels_are_high_density_ ? 14 : 7)));
			pixel_pointer_ = nullptr;
		}
};

}
}

#endif /* Video_hpp */
