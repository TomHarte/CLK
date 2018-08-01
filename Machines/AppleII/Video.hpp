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

#include <vector>

namespace AppleII {
namespace Video {

class BusHandler {
	public:
		/*!
			Reads an 8-bit value from the ordinary II/II+ memory pool.
		*/
		uint8_t perform_read(uint16_t address) {
			return 0xff;
		}

		/*!
			Reads two 8-bit values, from the same address — one from
			main RAM, one from auxiliary. Should return as
			(main) | (aux << 8).
		*/
		uint16_t perform_aux_read(uint16_t address) {
			return 0xffff;
		}
};

class VideoBase {
	public:
		VideoBase();

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
};

template <class BusHandler> class Video: public VideoBase {
	public:
		/// Constructs an instance of the video feed; a CRT is also created.
		Video(BusHandler &bus_handler) :
			VideoBase(),
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

							switch(line_mode) {
								case GraphicsMode::Text: {
									const uint8_t inverses[] = {
										0xff,
										static_cast<uint8_t>((flash_ / flash_length) * 0xff),
										0x00,
										0x00
									};
									for(int c = column_; c < pixel_end; ++c) {
										const uint8_t character = bus_handler_.perform_read(static_cast<uint16_t>(text_address + c));
										const std::size_t character_address = static_cast<std::size_t>(((character & 0x3f) << 3) + pixel_row);

										const uint8_t character_pattern = character_rom_[character_address] ^ inverses[character >> 6];

										// The character ROM is output MSB to LSB rather than LSB to MSB.
										pixel_pointer_[0] = character_pattern & 0x40;
										pixel_pointer_[1] = character_pattern & 0x20;
										pixel_pointer_[2] = character_pattern & 0x10;
										pixel_pointer_[3] = character_pattern & 0x08;
										pixel_pointer_[4] = character_pattern & 0x04;
										pixel_pointer_[5] = character_pattern & 0x02;
										pixel_pointer_[6] = character_pattern & 0x01;
										graphics_carry_ = character_pattern & 0x01;
										pixel_pointer_ += 7;
									}
								} break;

								case GraphicsMode::DoubleText: {
									const uint8_t inverses[] = {
										0xff,
										static_cast<uint8_t>((flash_ / flash_length) * 0xff),
										0x00,
										0x00
									};
									for(int c = column_; c < pixel_end; ++c) {
										const uint16_t characters = bus_handler_.perform_aux_read(static_cast<uint16_t>(text_address + c));
										const std::size_t character_addresses[2] = {
											static_cast<std::size_t>((((characters >> 8) & 0x3f) << 3) + pixel_row),
											static_cast<std::size_t>(((characters & 0x3f) << 3) + pixel_row),
										};

										const uint8_t character_patterns[2] = {
											static_cast<uint8_t>(character_rom_[character_addresses[0]] ^ inverses[(characters >> 14) & 3]),
											static_cast<uint8_t>(character_rom_[character_addresses[1]] ^ inverses[(characters >> 6) & 3]),
										};

										// The character ROM is output MSB to LSB rather than LSB to MSB.
										pixel_pointer_[0] = character_patterns[0] & 0x40;
										pixel_pointer_[1] = character_patterns[0] & 0x20;
										pixel_pointer_[2] = character_patterns[0] & 0x10;
										pixel_pointer_[3] = character_patterns[0] & 0x08;
										pixel_pointer_[4] = character_patterns[0] & 0x04;
										pixel_pointer_[5] = character_patterns[0] & 0x02;
										pixel_pointer_[6] = character_patterns[0] & 0x01;
										pixel_pointer_[7] = character_patterns[1] & 0x40;
										pixel_pointer_[8] = character_patterns[1] & 0x20;
										pixel_pointer_[9] = character_patterns[1] & 0x10;
										pixel_pointer_[10] = character_patterns[1] & 0x08;
										pixel_pointer_[11] = character_patterns[1] & 0x04;
										pixel_pointer_[12] = character_patterns[1] & 0x02;
										pixel_pointer_[13] = character_patterns[1] & 0x01;
										graphics_carry_ = character_patterns[1] & 0x01;
										pixel_pointer_ += 14;
									}
								} break;

								case GraphicsMode::DoubleLowRes: {
									const int row_shift = (row_&4);
									for(int c = column_; c < pixel_end; ++c) {
										const uint16_t nibble = (bus_handler_.perform_aux_read(static_cast<uint16_t>(text_address + c)) >> row_shift) & 0xf0f;

										if(c&1) {
											pixel_pointer_[0] = pixel_pointer_[4] = (nibble >> 8) & 4;
											pixel_pointer_[1] = pixel_pointer_[5] = (nibble >> 8) & 8;
											pixel_pointer_[2] = pixel_pointer_[6] = (nibble >> 8) & 1;
											pixel_pointer_[3] = (nibble >> 8) & 2;

											pixel_pointer_[8] = pixel_pointer_[12] = nibble & 4;
											pixel_pointer_[9] = pixel_pointer_[13] = nibble & 8;
											pixel_pointer_[10] = nibble & 1;
											pixel_pointer_[7] = pixel_pointer_[11] = nibble & 2;
											graphics_carry_ = nibble & 8;
										} else {
											pixel_pointer_[0] = pixel_pointer_[4] = (nibble >> 8) & 1;
											pixel_pointer_[1] = pixel_pointer_[5] = (nibble >> 8) & 2;
											pixel_pointer_[2] = pixel_pointer_[6] = (nibble >> 8) & 4;
											pixel_pointer_[3] = (nibble >> 8) & 8;

											pixel_pointer_[8] = pixel_pointer_[12] = nibble & 1;
											pixel_pointer_[9] = pixel_pointer_[13] = nibble & 2;
											pixel_pointer_[10] = nibble & 4;
											pixel_pointer_[7] = pixel_pointer_[11] = nibble & 8;
											graphics_carry_ = nibble & 2;
										}
										pixel_pointer_ += 14;
									}
								} break;

								case GraphicsMode::LowRes: {
									const int row_shift = (row_&4);
									// TODO: decompose into two loops, possibly.
									for(int c = column_; c < pixel_end; ++c) {
										const uint8_t nibble = (bus_handler_.perform_read(static_cast<uint16_t>(text_address + c)) >> row_shift) & 0x0f;

										// Low-resolution graphics mode shifts the colour code on a loop, but has to account for whether this
										// 14-sample output window is starting at the beginning of a colour cycle or halfway through.
										if(c&1) {
											pixel_pointer_[0] = pixel_pointer_[4] = pixel_pointer_[8] = pixel_pointer_[12] = nibble & 4;
											pixel_pointer_[1] = pixel_pointer_[5] = pixel_pointer_[9] = pixel_pointer_[13] = nibble & 8;
											pixel_pointer_[2] = pixel_pointer_[6] = pixel_pointer_[10] = nibble & 1;
											pixel_pointer_[3] = pixel_pointer_[7] = pixel_pointer_[11] = nibble & 2;
											graphics_carry_ = nibble & 8;
										} else {
											pixel_pointer_[0] = pixel_pointer_[4] = pixel_pointer_[8] = pixel_pointer_[12] = nibble & 1;
											pixel_pointer_[1] = pixel_pointer_[5] = pixel_pointer_[9] = pixel_pointer_[13] = nibble & 2;
											pixel_pointer_[2] = pixel_pointer_[6] = pixel_pointer_[10] = nibble & 4;
											pixel_pointer_[3] = pixel_pointer_[7] = pixel_pointer_[11] = nibble & 8;
											graphics_carry_ = nibble & 2;
										}
										pixel_pointer_ += 14;
									}
								} break;

								case GraphicsMode::HighRes: {
									const uint16_t graphics_address = static_cast<uint16_t>(((video_page()+1) * 0x2000) + row_address + ((pixel_row&7) << 10));
									for(int c = column_; c < pixel_end; ++c) {
										const uint8_t graphic = bus_handler_.perform_read(static_cast<uint16_t>(graphics_address + c));

										// High resolution graphics shift out LSB to MSB, optionally with a delay of half a pixel.
										// If there is a delay, the previous output level is held to bridge the gap.
										if(graphic & 0x80) {
											pixel_pointer_[0] = graphics_carry_;
											pixel_pointer_[1] = pixel_pointer_[2] = graphic & 0x01;
											pixel_pointer_[3] = pixel_pointer_[4] = graphic & 0x02;
											pixel_pointer_[5] = pixel_pointer_[6] = graphic & 0x04;
											pixel_pointer_[7] = pixel_pointer_[8] = graphic & 0x08;
											pixel_pointer_[9] = pixel_pointer_[10] = graphic & 0x10;
											pixel_pointer_[11] = pixel_pointer_[12] = graphic & 0x20;
											pixel_pointer_[13] = graphic & 0x40;
										} else {
											pixel_pointer_[0] = pixel_pointer_[1] = graphic & 0x01;
											pixel_pointer_[2] = pixel_pointer_[3] = graphic & 0x02;
											pixel_pointer_[4] = pixel_pointer_[5] = graphic & 0x04;
											pixel_pointer_[6] = pixel_pointer_[7] = graphic & 0x08;
											pixel_pointer_[8] = pixel_pointer_[9] = graphic & 0x10;
											pixel_pointer_[10] = pixel_pointer_[11] = graphic & 0x20;
											pixel_pointer_[12] = pixel_pointer_[13] = graphic & 0x40;
										}
										graphics_carry_ = graphic & 0x40;
										pixel_pointer_ += 14;
									}
								} break;

								case GraphicsMode::DoubleHighRes: {
									const uint16_t graphics_address = static_cast<uint16_t>(((video_page()+1) * 0x2000) + row_address + ((pixel_row&7) << 10));
									for(int c = column_; c < pixel_end; ++c) {
										const uint16_t graphic = bus_handler_.perform_aux_read(static_cast<uint16_t>(graphics_address + c));

										pixel_pointer_[0] = graphics_carry_;
										pixel_pointer_[1] = (graphic >> 8) & 0x01;
										pixel_pointer_[2] = (graphic >> 8) & 0x02;
										pixel_pointer_[3] = (graphic >> 8) & 0x04;
										pixel_pointer_[4] = (graphic >> 8) & 0x08;
										pixel_pointer_[5] = (graphic >> 8) & 0x10;
										pixel_pointer_[6] = (graphic >> 8) & 0x20;
										pixel_pointer_[7] = (graphic >> 8) & 0x40;
										pixel_pointer_[8] = graphic & 0x01;
										pixel_pointer_[9] = graphic & 0x02;
										pixel_pointer_[10] = graphic & 0x04;
										pixel_pointer_[11] = graphic & 0x08;
										pixel_pointer_[12] = graphic & 0x10;
										pixel_pointer_[13] = graphic & 0x20;
										graphics_carry_ = graphic & 0x40;
										pixel_pointer_ += 14;
									}
								} break;
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
							crt_->output_default_colour_burst(static_cast<unsigned int>(colour_burst_end - colour_burst_start) * 14);
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
			return bus_handler_.perform_read(read_address);
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

		static const int flash_length = 8406;
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
