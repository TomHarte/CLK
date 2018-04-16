//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
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
		uint8_t perform_read(uint16_t address) {
			return 0xff;
		}
};

template <class BusHandler> class Video {
	public:
		/// Constructs an instance of the video feed; a CRT is also created.
		Video(BusHandler &bus_handler) :
			bus_handler_(bus_handler),
			crt_(new Outputs::CRT::CRT(455, 1, Outputs::CRT::DisplayType::NTSC60, 1)) {

			// Set a composite sampling function that assumes 1bpp input, and uses just 7 bits per byte.
			crt_->set_composite_sampling_function(
				"float composite_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate, float phase, float amplitude)"
				"{"
//					"uint texValue = texture(sampler, coordinate).r;"
//					"texValue <<= int(icoordinate.x * 8) & 7;"
//					"return float(texValue & 128u);"
					"uint texValue = texture(sampler, coordinate).r;"
					"texValue <<= uint(icoordinate.x * 7.0) % 7u;"
					"return float(texValue & 64u);"
				"}");

				// TODO: the above has precision issues. Fix!

			// Show only the centre 75% of the TV frame.
			crt_->set_video_signal(Outputs::CRT::VideoSignal::Composite);
			crt_->set_visible_area(Outputs::CRT::Rect(0.115f, 0.117f, 0.77f, 0.77f));
		}

		/// @returns The CRT this video feed is feeding.
		Outputs::CRT::CRT *get_crt() {
			return crt_.get();
		}

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
			const int first_sync_line = 220;	// A complete guess. Information needed.
			const int first_sync_column = 49;	// Also a guess.

			int int_cycles = cycles.as_int();
			while(int_cycles) {
				const int cycles_this_line = std::min(65 - column_, int_cycles);

				if(row_ >= first_sync_line && row_ < first_sync_line + 3) {
					crt_->output_sync(static_cast<unsigned int>(cycles_this_line) * 7);
				} else {
					const int ending_column = column_ + cycles_this_line;

					// The first 40 columns are submitted to the CRT only upon completion;
					// they'll be either graphics or blank, depending on which side we are
					// of line 192.
					if(column_ < 40) {
						if(row_ < 192) {
							if(!column_) {
								pixel_pointer_ = crt_->allocate_write_area(40);
							}

							const int pixel_end = std::min(40, ending_column);
							const int character_row = row_ >> 3;
							const int pixel_row = row_ & 7;
							const uint16_t line_address = static_cast<uint16_t>(0x400 + (character_row >> 3) * 40 + ((character_row&7) << 7));

							for(int c = column_; c < pixel_end; ++c) {
								const uint16_t address = static_cast<uint16_t>(line_address + c);
								const uint8_t character = bus_handler_.perform_read(address);
								const int index = (character & 0x7f) << 3;

								const std::size_t character_address = static_cast<std::size_t>(index + pixel_row);
								pixel_pointer_[c] = character_rom_[character_address] ^ ((character & 0x80) ? 0x00 : 0xff);
							}

							if(ending_column >= 40) {
								crt_->output_data(280, 7);
							}
						} else {
							if(ending_column >= 40) {
								crt_->output_blank(280);
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
						crt_->output_blank(static_cast<unsigned int>(first_blank_end - first_blank_start) * 7);
					}

					// TODO: colour burst.

					const int sync_start = std::max(first_sync_column, column_);
					const int sync_end = std::min(first_sync_column + 4, ending_column);
					if(sync_end > sync_start) {
						crt_->output_sync(static_cast<unsigned int>(sync_end - sync_start) * 7);
					}

					const int second_blank_start = std::max(first_sync_column + 4, column_);
					if(ending_column > second_blank_start) {
						crt_->output_blank(static_cast<unsigned int>(ending_column - second_blank_start) * 7);
					}
				}

				int_cycles -= cycles_this_line;
				column_ = (column_ + cycles_this_line) % 65;
				if(!column_) {
					row_ = (row_ + 1) % 262;

					// Add an extra half a colour cycle of blank; this isn't counted in the run_for
					// count explicitly but is promised.
					crt_->output_blank(1);
				}
			}
		}

		// Inputs for the various soft switches.
		void set_graphics_mode() {
			printf("Graphics mode\n");
		}

		void set_text_mode() {
			printf("Text mode\n");
		}

		void set_mixed_mode(bool mixed_mode) {
			printf("Mixed mode: %s\n", mixed_mode ? "true" : "false");
		}

		void set_video_page(int page) {
			printf("Video page: %d\n", page);
		}

		void set_low_resolution() {
			printf("Low resolution\n");
		}

		void set_high_resolution() {
			printf("High resolution\n");
		}

		void set_character_rom(const std::vector<uint8_t> &character_rom) {
			character_rom_ = character_rom;
		}

	private:
		BusHandler &bus_handler_;
		std::unique_ptr<Outputs::CRT::CRT> crt_;

		int video_page_ = 0;
		int row_ = 0, column_ = 0;
		uint8_t *pixel_pointer_ = nullptr;
		std::vector<uint8_t> character_rom_;
};

}
}

#endif /* Video_hpp */
